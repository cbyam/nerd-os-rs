//! Differential tests of the configuration store (rust_core::config, C API
//! config_store.h) against the accessors of the former nvs_config.cpp on
//! ArduinoJson (cpp-shim/json_write_ref.cpp): getters on random documents,
//! random set sequences and the saved document.

use std::ffi::{c_char, CStr, CString};

use c_reference as _;

extern "C" {
    fn ref_cfg_load(json: *const c_char) -> bool;
    fn ref_cfg_get_u16(key: *const c_char, def: u16) -> u16;
    fn ref_cfg_get_u64(key: *const c_char, def: u64) -> u64;
    fn ref_cfg_get_bool(key: *const c_char, def: bool) -> bool;
    fn ref_cfg_get_str(key: *const c_char) -> *const c_char;
    fn ref_cfg_has(key: *const c_char) -> bool;
    fn ref_cfg_set_str(key: *const c_char, value: *const c_char);
    fn ref_cfg_set_u16(key: *const c_char, value: u16);
    fn ref_cfg_set_bool(key: *const c_char, value: bool);
    fn ref_cfg_set_u64(key: *const c_char, value: u64);
    fn ref_cfg_save(out: *mut c_char, cap: usize) -> usize;

    fn config_load(json: *const u8, len: usize) -> bool;
    fn config_get_u16(key: *const c_char, def: u16) -> u16;
    fn config_get_u64(key: *const c_char, def: u64) -> u64;
    fn config_get_bool(key: *const c_char, def: bool) -> bool;
    fn config_get_str(key: *const c_char, def: *const c_char) -> *mut c_char;
    fn config_has(key: *const c_char) -> bool;
    fn config_set_str(key: *const c_char, value: *const c_char);
    fn config_set_u16(key: *const c_char, value: u16);
    fn config_set_bool(key: *const c_char, value: bool);
    fn config_set_u64(key: *const c_char, value: u64);
    fn config_mark_dirty();
    fn config_flush(
        write: unsafe extern "C" fn(
            ctx: *mut std::ffi::c_void,
            json: *const u8,
            len: usize,
        ) -> bool,
        ctx: *mut std::ffi::c_void,
    ) -> bool;
    fn free(p: *mut std::ffi::c_void);
}

/// Both stores are global: one test at a time.
static LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }
    fn pick<'a, T>(&mut self, v: &'a [T]) -> &'a T {
        &v[(self.next() % v.len() as u64) as usize]
    }
}

const VALUES: &[&str] = &[
    "0",
    "1",
    "2",
    "65535",
    "65536",
    "70000",
    "-1",
    "-70000",
    "1.9",
    "-0.5",
    "65535.9",
    "65536.0",
    "1e3",
    "1e19",
    "18446744073709551615",
    "18446744073709551616",
    "9223372036854775807",
    "-9223372036854775808",
    "true",
    "false",
    "null",
    "\"\"",
    "\"42\"",
    "\"x\"",
    "\"1.5\"",
    "\" 7\"",
    "\"0x10\"",
    "\"a\\u0000b\"",
    "[]",
    "{}",
    "[1]",
    "{\"k\":1}",
    "4294967040.0",
    "4294967296.5",
    "1.8446742974197924e19",
];
const KEYS: &[&str] = &["a", "b", "c", "fanspeed", "missing"];

unsafe fn rust_saved() -> String {
    unsafe extern "C" fn w(ctx: *mut std::ffi::c_void, json: *const u8, len: usize) -> bool {
        let out = &mut *(ctx as *mut String);
        *out = String::from_utf8_lossy(std::slice::from_raw_parts(json, len)).into_owned();
        true
    }
    let mut out = String::new();
    config_mark_dirty();
    config_flush(w, &mut out as *mut String as *mut std::ffi::c_void);
    out
}

unsafe fn ref_saved() -> String {
    let mut buf = vec![0u8; 8192];
    let n = ref_cfg_save(buf.as_mut_ptr() as *mut c_char, buf.len());
    String::from_utf8_lossy(&buf[..n]).into_owned()
}

unsafe fn compare_getters(doc: &str) {
    for k in KEYS {
        let key = CString::new(*k).unwrap();
        let p = key.as_ptr();
        for def in [0u16, 7, 65535] {
            assert_eq!(
                config_get_u16(p, def),
                ref_cfg_get_u16(p, def),
                "u16 {k} in {doc}"
            );
        }
        for def in [0u64, 9, u64::MAX] {
            assert_eq!(
                config_get_u64(p, def),
                ref_cfg_get_u64(p, def),
                "u64 {k} in {doc}"
            );
        }
        for def in [false, true] {
            assert_eq!(
                config_get_bool(p, def),
                ref_cfg_get_bool(p, def),
                "bool {k} in {doc}"
            );
        }
        assert_eq!(config_has(p), ref_cfg_has(p), "has {k} in {doc}");
        let ours = config_get_str(p, std::ptr::null());
        let theirs = ref_cfg_get_str(p);
        assert_eq!(ours.is_null(), theirs.is_null(), "str {k} in {doc}");
        if !ours.is_null() {
            assert_eq!(
                CStr::from_ptr(ours),
                CStr::from_ptr(theirs),
                "str {k} in {doc}"
            );
            free(ours as *mut _);
        }
    }
}

#[test]
fn getters_on_random_documents() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let mut r = Rng(0xdead_beef_1234_5678);
    for _ in 0..20_000 {
        let mut doc = String::from("{");
        for i in 0..r.next() % 5 {
            if i > 0 {
                doc.push(',');
            }
            doc.push_str(&format!("\"{}\":{}", r.pick(KEYS), r.pick(VALUES)));
        }
        doc.push('}');
        let c = CString::new(doc.clone()).unwrap();
        unsafe {
            assert_eq!(
                config_load(doc.as_ptr(), doc.len()),
                ref_cfg_load(c.as_ptr()),
                "{doc}"
            );
            compare_getters(&doc);
            assert_eq!(rust_saved(), ref_saved(), "saved {doc}");
        }
    }
}

#[test]
fn random_set_sequences() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let mut r = Rng(0x0123_4567_89ab_cdef);
    let strings = [
        "",
        "x",
        "with \"quote\"",
        "back\\slash",
        "nl\nand\ttab",
        "äöü",
    ];
    for _ in 0..5_000 {
        unsafe {
            let doc = "{\"b\":1,\"fanspeed\":\"old\"}";
            let c = CString::new(doc).unwrap();
            config_load(doc.as_ptr(), doc.len());
            ref_cfg_load(c.as_ptr());
            for _ in 0..r.next() % 8 {
                let key = CString::new(*r.pick(KEYS)).unwrap();
                let k = key.as_ptr();
                match r.next() % 5 {
                    0 => {
                        let v = CString::new(*r.pick(&strings)).unwrap();
                        config_set_str(k, v.as_ptr());
                        ref_cfg_set_str(k, v.as_ptr());
                    }
                    1 => {
                        config_set_str(k, std::ptr::null());
                        ref_cfg_set_str(k, std::ptr::null());
                    }
                    2 => {
                        let v = r.next() as u16;
                        config_set_u16(k, v);
                        ref_cfg_set_u16(k, v);
                    }
                    3 => {
                        let v = r.next() % 2 == 0;
                        config_set_bool(k, v);
                        ref_cfg_set_bool(k, v);
                    }
                    _ => {
                        let v = r.next();
                        config_set_u64(k, v);
                        ref_cfg_set_u64(k, v);
                    }
                }
            }
            assert_eq!(rust_saved(), ref_saved());
            compare_getters("after sets");
        }
    }
}
