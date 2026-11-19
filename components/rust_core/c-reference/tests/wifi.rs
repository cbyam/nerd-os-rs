//! Differential tests of the WiFi station decisions: rust_core::wifi against
//! the former event handler of connect.cpp (c-wifi/, verbatim) on recording
//! shims: random sequences of start, disconnect, got-IP, pause/resume and
//! scan start/end give the same calls (connect, event bits, status, hooks,
//! backoff) in the same order and the same IP address. The Rust backoff is
//! a timer; here it fires right away, where the former code slept.

use std::ffi::{c_char, c_int, CStr};
use std::sync::Mutex;

use c_reference as _;
use rust_core::wifi::{act, ip_str, State};

extern "C" {
    fn ref_wifi_reset(has_ssid: bool);
    fn ref_wifi_event(ev: c_int, ip: u32);
    fn ref_wifi_trace(buf: *mut c_char, len: usize) -> usize;
    fn ref_wifi_ip(buf: *mut c_char, len: usize) -> bool;
}

// the former code is global state
static LOCK: Mutex<()> = Mutex::new(());

struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }
}

fn ref_trace() -> String {
    let mut b = [0 as c_char; 4096];
    unsafe { ref_wifi_trace(b.as_mut_ptr(), b.len()) };
    unsafe { CStr::from_ptr(b.as_ptr()) }
        .to_str()
        .unwrap()
        .to_owned()
}

/// the actions as the shims record them (`wifi_status_t` codes)
fn run(s: &mut State, a: u32, t: &mut String) {
    let mut p = |x: &str| {
        t.push_str(x);
        t.push(';');
    };
    if a & act::CLEAR_CONNECTED_BIT != 0 {
        p("clr_conn");
    }
    if a & act::HOOK_DISCONNECTED != 0 {
        p("hook_disc");
    }
    if a & act::SET_FAIL_BIT != 0 {
        p("set_fail");
    }
    if a & act::STATUS_FAILED != 0 {
        p("status4/0");
    }
    if a & act::CONNECT != 0 {
        p("connect");
    }
    if a & act::STATUS_RETRYING != 0 {
        p(&format!("status5/{}", s.retries));
    }
    if a & act::SET_CONNECTED_BIT != 0 {
        p("set_conn");
    }
    if a & act::STATUS_CONNECTED != 0 {
        p("status0/0");
    }
    if a & act::HOOK_GOT_IP != 0 {
        p("hook_ip");
    }
    if a & act::STATUS_PAUSED != 0 {
        p("status6/0");
    }
    if a & act::BACKOFF != 0 {
        p("backoff");
        let b = s.backoff_done();
        run(s, b, t);
    }
}

#[test]
fn event_sequences_match_the_former_handler() {
    let _g = LOCK.lock().unwrap();
    let mut rng = Rng(0x5eed_1234_abcd_0001);
    for seq in 0..3000 {
        let has_ssid = seq % 7 != 0;
        unsafe { ref_wifi_reset(has_ssid) };
        let mut s = State::default();
        s.has_ssid = has_ssid;
        for step in 0..40 {
            let ev = (rng.next() % 7) as c_int;
            let ip = rng.next() as u32;
            unsafe { ref_wifi_event(ev, ip) };
            let old = ref_trace();
            let mut new = String::new();
            let a = match ev {
                0 => s.sta_start(),
                1 => s.disconnected(),
                2 => s.got_ip(ip.to_le_bytes()),
                3 => s.pause(true),
                4 => s.pause(false),
                5 => {
                    s.scan_begin();
                    0
                }
                _ => s.scan_finish(),
            };
            run(&mut s, a, &mut new);
            let ctx = format!("seq {seq} step {step} ev {ev} state {s:?}");
            if (ev == 6 || ev == 0) && s.paused && has_ssid {
                // intended: no esp_wifi_connect() on the paused (stopped)
                // WiFi; the former code called it after a scan
                // (start does not happen while stopped)
                assert_eq!(new, "", "{ctx}");
                assert!(old == "connect;" || old.is_empty(), "{ctx}: {old}");
            } else {
                assert_eq!(new, old, "{ctx}");
            }
            let mut b = [0 as c_char; 20];
            let valid = unsafe { ref_wifi_ip(b.as_mut_ptr(), b.len()) };
            let old_ip = unsafe { CStr::from_ptr(b.as_ptr()) }.to_str().unwrap();
            assert_eq!(valid, s.ip.is_some(), "{ctx}");
            assert_eq!(ip_str(s.ip), old_ip, "{ctx}");
        }
    }
}
