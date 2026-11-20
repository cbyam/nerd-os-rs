//! Settings profiles: up to [`MAX_PROFILES`] named sets of settings that a
//! home automation switches between (e.g. overclocked while the PV system
//! has a surplus, normal otherwise). A profile holds a subset of the
//! `PATCH /api/v2/settings` body (see [`KEYS`]); applying it goes the same
//! way as that PATCH, so frequency, voltage, fans, pool mode and balance
//! take effect without a restart.
//!
//! The profiles are one JSON array in their own NVS blob; applying one only
//! writes the normal config (and the index of the profile last applied).
//!
//! Applying needs no OTP: it only selects one of the stored profiles.
//! Changing the profiles needs OTP like any other setting.

use core::ffi::CStr;
use std::sync::Mutex;

use crate::ajson::{self, Json, Num};
use crate::config;
use crate::log::log;

const TAG: &CStr = c"profiles";

pub const MAX_PROFILES: usize = 5;
pub const MAX_NAME: usize = 32;
/// Upper bound of the stored JSON (NVS space).
pub const MAX_BLOB: usize = 2048;

/// The settings a profile may hold (`PATCH /api/v2/settings` names).
pub const KEYS: [&str; 5] = [
    "frequency",
    "coreVoltage",
    "fans",
    "poolMode",
    "poolBalance",
];
/// The members of a fan channel, and of its `pid` object.
const FAN_KEYS: [&str; 3] = ["mode", "manualSpeed", "overheatTemp"];
const PID_KEYS: [&str; 4] = ["targetTemp", "p", "i", "d"];

/// The config key of the profile last applied (none: absent).
const ACTIVE_KEY: &[u8] = b"profile_act";

#[derive(Clone, Debug, PartialEq)]
pub struct Profile {
    pub name: Vec<u8>,
    /// an object with members of [`KEYS`] only
    pub settings: Json,
}

/// Why a profile list was refused (the 400 message).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    InvalidJson,
    NotAList,
    TooMany,
    BadName,
    BadSettings,
    TooLarge,
}

impl Error {
    pub fn message(self) -> &'static CStr {
        match self {
            Error::InvalidJson => c"Invalid JSON",
            Error::NotAList => c"Expected {\"profiles\": [...]}",
            Error::TooMany => c"Too many profiles (at most 5)",
            Error::BadName => c"Each profile needs a name (1 to 32 characters)",
            Error::BadSettings => c"Invalid profile settings",
            Error::TooLarge => c"Profiles too large",
        }
    }
}

fn members(v: &Json) -> &[(Vec<u8>, Json)] {
    match v {
        Json::Obj(m) => m,
        _ => &[],
    }
}

/// The members of `v` named in `keys` (in the order received).
fn pick(v: &Json, keys: &[&str]) -> Vec<(Vec<u8>, Json)> {
    members(v)
        .iter()
        .filter(|(k, _)| keys.iter().any(|key| key.as_bytes() == k.as_slice()))
        .cloned()
        .collect()
}

fn is_uint(v: &Json, min: u64, max: u64) -> bool {
    v.as_uint(max).is_some_and(|n| n >= min)
}

fn is_number(v: &Json) -> bool {
    matches!(v, Json::Num(_))
}

/// A fan channel restricted to its known members, or None if invalid.
fn fan(v: &Json) -> Option<Json> {
    if !v.is_object() {
        return None;
    }
    let mut out = pick(v, &FAN_KEYS);
    let ok = out.iter().all(|(k, v)| match k.as_slice() {
        b"mode" => is_uint(v, 0, 2),
        b"manualSpeed" => is_uint(v, 0, 100),
        _ => is_uint(v, 1, 0xffff),
    });
    if !ok {
        return None;
    }
    let pid = v.get("pid");
    if !pid.is_null() {
        if !pid.is_object() {
            return None;
        }
        let p = pick(pid, &PID_KEYS);
        let ok = p.iter().all(|(k, v)| match k.as_slice() {
            b"targetTemp" => is_uint(v, 1, 0xffff),
            _ => is_number(v),
        });
        if !ok {
            return None;
        }
        out.push((b"pid".to_vec(), Json::Obj(p)));
    }
    Some(Json::Obj(out))
}

/// The settings of a profile restricted to [`KEYS`], or None if a value is
/// invalid. Unknown members are dropped.
pub fn settings(v: &Json) -> Option<Json> {
    if !v.is_object() {
        return None;
    }
    let mut out = Vec::new();
    for (k, v) in pick(v, &KEYS) {
        let v = match k.as_slice() {
            b"frequency" | b"coreVoltage" => is_uint(&v, 1, 0xffff).then_some(v)?,
            b"poolMode" => is_uint(&v, 0, 1).then_some(v)?,
            b"poolBalance" => is_uint(&v, 1, 99).then_some(v)?,
            _ => {
                let fans = v.as_array()?;
                if fans.len() > 2 {
                    return None;
                }
                Json::Arr(fans.iter().map(fan).collect::<Option<_>>()?)
            }
        };
        out.push((k, v));
    }
    Some(Json::Obj(out))
}

fn profile(v: &Json) -> Result<Profile, Error> {
    let name = v.get("name").as_cstr().ok_or(Error::BadName)?;
    if name.is_empty() || name.len() > MAX_NAME || name.contains(&0) {
        return Err(Error::BadName);
    }
    let settings = settings(v.get("settings")).ok_or(Error::BadSettings)?;
    Ok(Profile {
        name: name.to_vec(),
        settings,
    })
}

/// The stored form: an array of `{"name", "settings"}`.
pub fn serialize(list: &[Profile]) -> Vec<u8> {
    Json::Arr(
        list.iter()
            .map(|p| {
                Json::Obj(vec![
                    (b"name".to_vec(), Json::Str(p.name.clone())),
                    (b"settings".to_vec(), p.settings.clone()),
                ])
            })
            .collect(),
    )
    .serialize()
}

/// The profiles of a `PATCH /api/v2/profiles` body (`{"profiles": [...]}`).
pub fn parse_patch(body: &[u8]) -> Result<Vec<Profile>, Error> {
    let doc = ajson::deserialize(body).map_err(|_| Error::InvalidJson)?;
    let items = doc.get("profiles").as_array().ok_or(Error::NotAList)?;
    if items.len() > MAX_PROFILES {
        return Err(Error::TooMany);
    }
    let list = items.iter().map(profile).collect::<Result<Vec<_>, _>>()?;
    if serialize(&list).len() > MAX_BLOB {
        return Err(Error::TooLarge);
    }
    Ok(list)
}

/// The stored form back (invalid entries are skipped).
pub fn parse_stored(data: &[u8]) -> Vec<Profile> {
    let Ok(doc) = ajson::deserialize(data) else {
        return Vec::new();
    };
    doc.as_array()
        .unwrap_or_default()
        .iter()
        .filter_map(|p| profile(p).ok())
        .take(MAX_PROFILES)
        .collect()
}

/// The profile of a `POST /api/v2/profiles/apply` body (`{"id": n}`).
pub fn parse_apply(body: &[u8], count: usize) -> Option<usize> {
    let doc = ajson::deserialize(body).ok()?;
    let id = doc.get("id").as_uint(MAX_PROFILES as u64)? as usize;
    (id < count).then_some(id)
}

/// `GET /api/v2/profiles`.
pub fn to_json(list: &[Profile], active: Option<usize>) -> Json {
    let profiles = list
        .iter()
        .enumerate()
        .map(|(i, p)| {
            Json::Obj(vec![
                (b"id".to_vec(), Json::Num(Num::U(i as u64))),
                (b"name".to_vec(), Json::Str(p.name.clone())),
                (b"settings".to_vec(), p.settings.clone()),
            ])
        })
        .collect();
    Json::Obj(vec![
        (b"max".to_vec(), Json::Num(Num::U(MAX_PROFILES as u64))),
        (
            b"active".to_vec(),
            active.map_or(Json::Null, |i| Json::Num(Num::U(i as u64))),
        ),
        (b"profiles".to_vec(), Json::Arr(profiles)),
    ])
}

/// The index of the profile last applied, if it still exists.
pub fn active(count: usize) -> Option<usize> {
    if !config::has(ACTIVE_KEY) {
        return None;
    }
    let i = config::get_u16(ACTIVE_KEY, 0) as usize;
    (i < count).then_some(i)
}

// ---------------------------------------------------------------------------
// storage

/// The loaded profiles (None: not read from NVS yet).
static PROFILES: Mutex<Option<Vec<Profile>>> = Mutex::new(None);

fn with<T>(f: impl FnOnce(&mut Vec<Profile>) -> T) -> T {
    let mut p = PROFILES.lock().unwrap_or_else(|e| e.into_inner());
    let list = p.get_or_insert_with(|| parse_stored(&nvs::read()));
    f(list)
}

#[cfg(target_os = "espidf")]
mod nvs {
    use super::*;
    use crate::idf::nvs::{err_name, Nvs, READONLY, READWRITE};
    use crate::idf::ESP_OK;

    const NAMESPACE: &CStr = c"main";
    const KEY: &CStr = c"profiles";

    pub fn read() -> Vec<u8> {
        Nvs::open(NAMESPACE, READONLY)
            .and_then(|n| n.get_blob(KEY))
            .unwrap_or_default()
    }

    pub fn write(data: &[u8]) -> bool {
        let nvs = match Nvs::open(NAMESPACE, READWRITE) {
            Ok(n) => n,
            Err(e) => {
                log!(Error, TAG, "nvs_open failed: {}", err_name(e));
                return false;
            }
        };
        let e = nvs.set_blob(KEY, data);
        let e = if e == ESP_OK { nvs.commit() } else { e };
        if e != ESP_OK {
            log!(Error, TAG, "saving the profiles failed: {}", err_name(e));
            return false;
        }
        true
    }
}

/// On the host: kept in memory.
#[cfg(not(target_os = "espidf"))]
mod nvs {
    use std::sync::Mutex;

    static BLOB: Mutex<Vec<u8>> = Mutex::new(Vec::new());

    pub fn read() -> Vec<u8> {
        BLOB.lock().unwrap().clone()
    }

    pub fn write(data: &[u8]) -> bool {
        *BLOB.lock().unwrap() = data.to_vec();
        true
    }
}

/// The stored profiles and the one last applied.
pub fn list() -> (Vec<Profile>, Option<usize>) {
    with(|l| (l.clone(), active(l.len())))
}

/// Replaces the profiles; false if they could not be saved.
pub fn store(new: Vec<Profile>) -> bool {
    with(|l| {
        if !nvs::write(&serialize(&new)) {
            return false;
        }
        log!(Info, TAG, "{} profile(s) saved", new.len());
        *l = new;
        true
    })
}

/// Applies profile `id` to the config (the caller persists and reloads);
/// its name, or None if there is no such profile.
pub fn apply(id: usize, s: &mut impl crate::settings_apply::Store) -> Option<Vec<u8>> {
    let p = with(|l| l.get(id).cloned())?;
    let mut applied = crate::settings_apply::Applied::default();
    let pid_use_max = crate::settings_apply::pid_use_max_board();
    for w in crate::http_settings::settings_v2(&p.settings) {
        crate::settings_apply::apply(&w, &mut applied, pid_use_max, s);
    }
    s.set_u16(ACTIVE_KEY, id as u16);
    log!(
        Info,
        TAG,
        "profile {id} applied: {}",
        String::from_utf8_lossy(&p.name)
    );
    Some(p.name)
}

// ---------------------------------------------------------------------------
// HTTP

pub mod http {
    use super::*;
    use crate::http_server::ServerCtx;
    use crate::idf::httpd::{self, err, Req};
    use crate::idf::{EspErr, ESP_FAIL, ESP_OK};

    unsafe fn send_err(req: *mut Req, code: httpd::ErrCode, msg: &CStr) -> EspErr {
        httpd::httpd_resp_send_err(req, code, msg.as_ptr())
    }

    unsafe fn send_doc(req: *mut Req, doc: Json) -> EspErr {
        httpd::httpd_resp_set_type(req, c"application/json".as_ptr());
        let mut len = 0;
        let p = crate::cutil::malloc_copy(&doc.serialize(), &mut len);
        crate::http_server::send_json(req, p, len)
    }

    /// The request body (read into the server context's scratch buffer).
    unsafe fn body<'a>(req: *mut Req) -> Option<&'a [u8]> {
        if crate::http_server::post_data(req) != ESP_OK {
            return None;
        }
        let ctx = (*req).user_ctx as *const ServerCtx;
        Some(CStr::from_ptr((*ctx).scratch.as_ptr()).to_bytes())
    }

    /// `GET /api/v2/profiles`
    ///
    /// # Safety
    /// `req` a request of a route with the server context.
    pub unsafe extern "C" fn get(req: *mut Req) -> EspErr {
        let (l, a) = list();
        send_doc(req, to_json(&l, a))
    }

    /// `PATCH /api/v2/profiles` (OTP): replaces all profiles.
    ///
    /// # Safety
    /// `req` a request of a route with the server context.
    pub unsafe extern "C" fn patch(req: *mut Req) -> EspErr {
        let Some(b) = body(req) else {
            return ESP_FAIL;
        };
        let new = match parse_patch(b) {
            Ok(n) => n,
            Err(e) => return send_err(req, err::E400_BAD_REQUEST, e.message()),
        };
        if !store(new) {
            return send_err(
                req,
                err::E500_INTERNAL_SERVER_ERROR,
                c"Saving the profiles failed",
            );
        }
        get(req)
    }

    /// `POST /api/v2/profiles/apply` with `{"id": n}` (no OTP: it only
    /// selects a stored profile).
    ///
    /// # Safety
    /// `req` a request of a route with the server context.
    pub unsafe extern "C" fn apply(req: *mut Req) -> EspErr {
        let Some(b) = body(req) else {
            return ESP_FAIL;
        };
        let count = list().0.len();
        let Some(id) = parse_apply(b, count) else {
            return send_err(req, err::E400_BAD_REQUEST, c"Unknown profile id");
        };
        let Some(name) = super::apply(id, &mut crate::settings_apply::Config) else {
            return send_err(req, err::E400_BAD_REQUEST, c"Unknown profile id");
        };
        crate::config_nvs::request_flush();
        let r = send_doc(
            req,
            Json::Obj(vec![
                (b"id".to_vec(), Json::Num(Num::U(id as u64))),
                (b"name".to_vec(), Json::Str(name)),
            ]),
        );
        crate::http_data::reload_settings();
        r
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const OC: &str = r#"{"name":"OC","settings":{"frequency":1000,"coreVoltage":1350,
        "poolMode":1,"poolBalance":70,"hostname":"x","fans":[{"mode":2,"pid":{"targetTemp":60,"p":6,"x":1},"y":2}]}}"#;

    #[test]
    fn keeps_only_profile_settings() {
        let list = parse_patch(format!(r#"{{"profiles":[{OC}]}}"#).as_bytes()).unwrap();
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].name, b"OC");
        assert_eq!(
            String::from_utf8(list[0].settings.serialize()).unwrap(),
            r#"{"frequency":1000,"coreVoltage":1350,"poolMode":1,"poolBalance":70,"fans":[{"mode":2,"pid":{"targetTemp":60,"p":6}}]}"#
        );
        // stored and read back unchanged
        assert_eq!(parse_stored(&serialize(&list)), list);
    }

    #[test]
    fn refuses_invalid_lists() {
        let p = |s: &str| parse_patch(s.as_bytes());
        assert_eq!(p("{"), Err(Error::InvalidJson));
        assert_eq!(p(r#"{"profiles":{}}"#), Err(Error::NotAList));
        let six = [r#"{"name":"a","settings":{}}"#; 6].join(",");
        assert_eq!(
            p(&format!(r#"{{"profiles":[{six}]}}"#)),
            Err(Error::TooMany)
        );
        assert_eq!(
            p(r#"{"profiles":[{"name":"","settings":{}}]}"#),
            Err(Error::BadName)
        );
        assert_eq!(
            p(&format!(
                r#"{{"profiles":[{{"name":"{}","settings":{{}}}}]}}"#,
                "n".repeat(33)
            )),
            Err(Error::BadName)
        );
        for bad in [
            r#"{"frequency":0}"#,
            r#"{"frequency":"600"}"#,
            r#"{"poolMode":2}"#,
            r#"{"poolBalance":100}"#,
            r#"{"fans":[{},{},{}]}"#,
            r#"{"fans":[{"manualSpeed":101}]}"#,
            r#"{"fans":[{"pid":{"p":"x"}}]}"#,
            "[]",
        ] {
            assert_eq!(
                p(&format!(
                    r#"{{"profiles":[{{"name":"a","settings":{bad}}}]}}"#
                )),
                Err(Error::BadSettings),
                "{bad}"
            );
        }
        assert_eq!(p(r#"{"profiles":[]}"#), Ok(vec![]));
    }

    #[test]
    fn apply_id_must_exist() {
        assert_eq!(parse_apply(br#"{"id":1}"#, 2), Some(1));
        assert_eq!(parse_apply(br#"{"id":2}"#, 2), None);
        assert_eq!(parse_apply(br#"{"id":-1}"#, 2), None);
        assert_eq!(parse_apply(b"{}", 2), None);
    }

    #[derive(Default)]
    struct Rec(Vec<String>);

    impl crate::settings_apply::Store for Rec {
        fn set_u16(&mut self, key: &[u8], v: u16) {
            self.0.push(format!("{}={v}", String::from_utf8_lossy(key)));
        }
        fn set_u64(&mut self, key: &[u8], v: u64) {
            self.0.push(format!("{}={v}", String::from_utf8_lossy(key)));
        }
        fn set_str(&mut self, _: &[u8], _: Option<&[u8]>) {}
        fn get_u16(&mut self, _: &[u8], def: u16) -> u16 {
            def
        }
    }

    #[test]
    fn applies_through_the_settings_path() {
        let _g = crate::config::tests::LOCK
            .lock()
            .unwrap_or_else(|e| e.into_inner());
        let list = parse_patch(
            br#"{"profiles":[{"name":"Normal","settings":{"frequency":600,"coreVoltage":1120}},
                {"name":"Dual","settings":{"poolMode":1,"poolBalance":30}}]}"#,
        )
        .unwrap();
        assert!(store(list));
        let mut r = Rec::default();
        assert_eq!(apply(1, &mut r).as_deref(), Some(&b"Dual"[..]));
        assert_eq!(r.0, ["pool_mode=1", "pool_balance=30", "profile_act=1"]);
        let mut r = Rec::default();
        assert_eq!(apply(0, &mut r).as_deref(), Some(&b"Normal"[..]));
        assert!(
            r.0.iter().any(|w| w.ends_with("=600")) && r.0.iter().any(|w| w.ends_with("=1120"))
        );
        assert!(apply(2, &mut Rec::default()).is_none());
        let doc = to_json(&list_only(), Some(0)).serialize();
        assert!(String::from_utf8(doc)
            .unwrap()
            .starts_with(r#"{"max":5,"active":0,"profiles":[{"id":0,"name":"Normal""#));
        assert!(store(vec![]));
    }

    fn list_only() -> Vec<Profile> {
        list().0
    }
}
