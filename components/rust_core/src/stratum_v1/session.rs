//! Request/response bookkeeping of a Stratum v1 connection: message IDs and
//! which request each pending ID belongs to, so responses are classified by
//! what was actually sent.
//!
//! Responses to pending IDs are classified by their request (without
//! `mining.extranonce.subscribe`, the first share gets ID 5, inside the
//! fixed setup range). IDs that are not pending (lost or unexpected
//! responses) follow the fixed rule: 1..=5 are setup requests, everything
//! above a share result.

use std::collections::VecDeque;

use super::{parse, request, Message};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Request {
    Subscribe,
    Configure,
    Authorize,
    SuggestDifficulty,
    ExtranonceSubscribe,
    Submit,
}

impl Request {
    /// The former fixed classification of a response ID.
    pub(super) fn legacy(id: i64) -> Option<Request> {
        Some(match id {
            1 => Request::Subscribe,
            2 => Request::Configure,
            3 => Request::Authorize,
            4 => Request::SuggestDifficulty,
            5 => Request::ExtranonceSubscribe,
            6.. => Request::Submit,
            _ => return None,
        })
    }
}

/// Pending requests kept for matching; older ones fall back to the fixed rule.
pub const MAX_PENDING: usize = 64;

pub struct Session {
    next_id: i32,
    pending: VecDeque<(i64, Request)>,
}

impl Default for Session {
    fn default() -> Self {
        Self {
            next_id: 1,
            pending: VecDeque::new(),
        }
    }
}

impl Session {
    /// Starts over for a new connection: IDs from 1, nothing pending.
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    fn track(&mut self, kind: Request) -> i32 {
        let id = self.next_id;
        self.next_id = self.next_id.checked_add(1).unwrap_or(1);
        if self.pending.len() == MAX_PENDING {
            self.pending.pop_front();
        }
        self.pending.push_back((id as i64, kind));
        id
    }

    /// Undoes the last `track()` (for a request that was not sent). A request
    /// dropped by the pending limit stays dropped; it falls back to the fixed
    /// rule then.
    pub(crate) fn untrack_last(&mut self) {
        if let Some((id, _)) = self.pending.pop_back() {
            self.next_id = id as i32;
        }
    }

    fn take(&mut self, id: i64) -> Option<Request> {
        let i = self.pending.iter().position(|&(p, _)| p == id)?;
        self.pending.remove(i).map(|(_, kind)| kind)
    }

    pub fn subscribe(&mut self, device: &[u8], asic: &[u8], version: &[u8]) -> Vec<u8> {
        request::subscribe(self.track(Request::Subscribe), device, asic, version)
    }

    pub fn configure_version_rolling(&mut self) -> Vec<u8> {
        request::configure_version_rolling(self.track(Request::Configure))
    }

    pub fn authorize(&mut self, user: &[u8], password: &[u8]) -> Vec<u8> {
        request::authorize(self.track(Request::Authorize), user, password)
    }

    pub fn suggest_difficulty(&mut self, difficulty: u32) -> Vec<u8> {
        request::suggest_difficulty(self.track(Request::SuggestDifficulty), difficulty)
    }

    pub fn extranonce_subscribe(&mut self) -> Vec<u8> {
        request::extranonce_subscribe(self.track(Request::ExtranonceSubscribe))
    }

    /// `version` is the rolled version delta, as `mining.submit` expects.
    pub fn submit(
        &mut self,
        user: &[u8],
        job_id: &[u8],
        extranonce2: &[u8],
        ntime: u32,
        nonce: u32,
        version: u32,
    ) -> Vec<u8> {
        let id = self.track(Request::Submit);
        request::submit(id, user, job_id, extranonce2, ntime, nonce, version)
    }

    /// Parses a line; a response is classified by the request it answers.
    pub fn parse(&mut self, line: &[u8], message: &mut Message) -> bool {
        parse::parse_with(line, message, &mut |id| {
            self.take(id).or_else(|| Request::legacy(id))
        })
    }
}

pub(crate) mod ffi {
    use super::*;
    use core::ffi::{c_char, CStr};
    use std::sync::Mutex;

    /// Shared by the task that owns the connection and the task submitting
    /// shares, hence the lock.
    pub struct Handle(pub(crate) Mutex<Session>);

    impl Handle {
        pub(crate) fn with<T>(&self, f: impl FnOnce(&mut Session) -> T) -> T {
            let mut s = self.0.lock().unwrap_or_else(|e| e.into_inner());
            f(&mut s)
        }
    }

    unsafe fn bytes<'a>(p: *const c_char) -> &'a [u8] {
        if p.is_null() {
            &[]
        } else {
            CStr::from_ptr(p).to_bytes()
        }
    }

    /// Writes `request` plus NUL into `buf`; returns its length, or 0 if it
    /// does not fit (then the request is not counted as pending either).
    unsafe fn write(
        handle: &Handle,
        buf: *mut c_char,
        buf_len: usize,
        f: impl FnOnce(&mut Session) -> Vec<u8>,
    ) -> usize {
        handle.with(|s| {
            let request = f(s);
            if buf.is_null() || request.len() + 1 > buf_len {
                s.untrack_last();
                return 0;
            }
            core::ptr::copy_nonoverlapping(request.as_ptr(), buf as *mut u8, request.len());
            *buf.add(request.len()) = 0;
            request.len()
        })
    }

    #[no_mangle]
    pub extern "C" fn stratum_v1_session_new() -> *mut Handle {
        Box::into_raw(Box::new(Handle(Mutex::new(Session::default()))))
    }

    /// # Safety
    /// `s` must come from `stratum_v1_session_new()` (or be NULL).
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_free(s: *mut Handle) {
        if !s.is_null() {
            drop(Box::from_raw(s));
        }
    }

    /// # Safety
    /// `s` must be a valid handle, `line` NUL-terminated, `message` zeroed.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_parse(
        s: *mut Handle,
        line: *const c_char,
        message: *mut Message,
    ) -> bool {
        if line.is_null() || message.is_null() {
            return false;
        }
        let ok = (*s).with(|session| session.parse(bytes(line), &mut *message));
        log_parsed(ok, &*message);
        ok
    }

    /// The log lines of the former `StratumApi::parse()`.
    pub(crate) unsafe fn log_parsed(ok: bool, m: &Message) {
        use super::super::method::*;
        use crate::log::log;
        const TAG: &CStr = c"stratum_api";
        match m.method {
            MINING_NOTIFY => log!(Info, TAG, "mining notify"),
            MINING_SET_EXTRANONCE | STRATUM_RESULT_SUBSCRIBE if ok => {
                let en = if m.extranonce_str.is_null() {
                    "".into()
                } else {
                    CStr::from_ptr(m.extranonce_str).to_string_lossy()
                };
                log!(Info, TAG, "extranonce_str: {en}");
                log!(Info, TAG, "extranonce_2_len: {}", m.extranonce_2_len);
            }
            MINING_SET_EXTRANONCE | STRATUM_RESULT_SUBSCRIBE => log!(
                Error,
                TAG,
                "Invalid extranonce (extranonce_2_len {})",
                m.extranonce_2_len
            ),
            STRATUM_RESULT_VERSION_MASK if ok => {
                log!(Info, TAG, "Set version mask: {:08x}", m.version_mask)
            }
            STRATUM_UNKNOWN => log!(Warn, TAG, "Unhandled stratum message"),
            _ => {}
        }
    }

    /// # Safety
    /// `s` must be a valid handle and `buf` hold `buf_len` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_subscribe(
        s: *mut Handle,
        buf: *mut c_char,
        buf_len: usize,
        device: *const c_char,
        asic: *const c_char,
        version: *const c_char,
    ) -> usize {
        write(&*s, buf, buf_len, |session| {
            session.subscribe(bytes(device), bytes(asic), bytes(version))
        })
    }

    /// # Safety
    /// `s` must be a valid handle and `buf` hold `buf_len` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_configure(
        s: *mut Handle,
        buf: *mut c_char,
        buf_len: usize,
    ) -> usize {
        write(&*s, buf, buf_len, Session::configure_version_rolling)
    }

    /// # Safety
    /// `s` must be a valid handle and `buf` hold `buf_len` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_authorize(
        s: *mut Handle,
        buf: *mut c_char,
        buf_len: usize,
        user: *const c_char,
        password: *const c_char,
    ) -> usize {
        write(&*s, buf, buf_len, |session| {
            session.authorize(bytes(user), bytes(password))
        })
    }

    /// # Safety
    /// `s` must be a valid handle and `buf` hold `buf_len` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_suggest_difficulty(
        s: *mut Handle,
        buf: *mut c_char,
        buf_len: usize,
        difficulty: u32,
    ) -> usize {
        write(&*s, buf, buf_len, |session| {
            session.suggest_difficulty(difficulty)
        })
    }

    /// # Safety
    /// `s` must be a valid handle and `buf` hold `buf_len` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_session_extranonce_subscribe(
        s: *mut Handle,
        buf: *mut c_char,
        buf_len: usize,
    ) -> usize {
        write(&*s, buf, buf_len, Session::extranonce_subscribe)
    }
}

#[cfg(test)]
mod tests {
    use super::super::method;
    use super::*;

    fn parse_method(s: &mut Session, json: &str) -> (bool, i32) {
        let mut m = Message::zeroed();
        let ok = s.parse(json.as_bytes(), &mut m);
        assert!(m.mining_notification.is_null() && m.extranonce_str.is_null());
        (ok, m.method)
    }

    #[test]
    fn ids_and_matching() {
        let mut s = Session::default();
        assert!(String::from_utf8(s.subscribe(b"d", b"a", b"v"))
            .unwrap()
            .starts_with("{\"id\": 1,"));
        s.configure_version_rolling();
        s.authorize(b"u", b"p");
        s.suggest_difficulty(1000);
        // no extranonce.subscribe: the first share gets ID 5
        assert!(String::from_utf8(s.submit(b"u", b"j", b"00", 1, 2, 3))
            .unwrap()
            .starts_with("{\"id\": 5,"));

        assert_eq!(
            parse_method(&mut s, r#"{"id":3,"result":true,"error":null}"#),
            (true, method::STRATUM_RESULT_SETUP)
        );
        // ID 5 is pending as a share here, so its result is a share result
        assert_eq!(
            parse_method(&mut s, r#"{"id":5,"result":true,"error":null}"#),
            (true, method::STRATUM_RESULT)
        );
        // a second answer to ID 5 is unexpected: fixed rule
        assert_eq!(
            parse_method(&mut s, r#"{"id":5,"result":true,"error":null}"#),
            (true, method::STRATUM_RESULT_SETUP)
        );
        // unknown IDs: fixed rule
        assert_eq!(
            parse_method(&mut s, r#"{"id":77,"result":false,"error":null}"#),
            (true, method::STRATUM_RESULT)
        );
        assert_eq!(
            parse_method(&mut s, r#"{"id":0,"result":true}"#),
            (false, method::STRATUM_UNKNOWN)
        );
    }

    #[test]
    fn reset_and_pending_limit() {
        let mut s = Session::default();
        for _ in 0..MAX_PENDING + 10 {
            s.submit(b"u", b"j", b"00", 0, 0, 0);
        }
        assert_eq!(s.pending.len(), MAX_PENDING);
        // the oldest submits (IDs 1..=10) were dropped: fixed rule
        assert_eq!(
            parse_method(&mut s, r#"{"id":2,"result":true}"#).1,
            method::STRATUM_RESULT_VERSION_MASK
        );
        assert_eq!(
            parse_method(&mut s, r#"{"id":11,"result":true}"#).1,
            method::STRATUM_RESULT
        );

        s.reset();
        assert!(s.pending.is_empty());
        assert!(String::from_utf8(s.extranonce_subscribe())
            .unwrap()
            .starts_with("{\"id\": 1,"));
    }

    /// The connection task and the submitting task use one handle.
    #[test]
    fn concurrent_submits_get_unique_ids() {
        use std::collections::HashSet;
        let handle = ffi::stratum_v1_session_new() as usize;
        let threads: Vec<_> = (0..4)
            .map(|_| {
                std::thread::spawn(move || {
                    let mut ids = Vec::new();
                    for _ in 0..500 {
                        // SAFETY: a valid handle until freed below.
                        let h = unsafe { &*(handle as *const ffi::Handle) };
                        let line = h.with(|s| s.submit(b"u", b"j", b"00", 0, 0, 0));
                        let v: serde_json::Value = serde_json::from_slice(&line).unwrap();
                        ids.push(v["id"].as_i64().unwrap());
                    }
                    ids
                })
            })
            .collect();
        let ids: Vec<i64> = threads
            .into_iter()
            .flat_map(|t| t.join().unwrap())
            .collect();
        assert_eq!(ids.iter().collect::<HashSet<_>>().len(), 2000);
        // SAFETY: created above, no longer used.
        unsafe { ffi::stratum_v1_session_free(handle as *mut _) };
    }
}
