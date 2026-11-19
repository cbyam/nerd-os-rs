//! Differential tests of the factory update inputs: the former URL check
//! and body handling (c-http/ota_ref.cpp, verbatim, with ArduinoJson)
//! against rust_core::http_ota.

use std::cell::Cell;
use std::ffi::{c_char, CStr, CString};

use c_reference as _;
use rust_core::http_ota::{factory_body, factory_url_ok, FactoryBody};

extern "C" {
    fn ref_factory_url_ok(url: *const c_char) -> bool;
    fn ref_factory_body(body: *const c_char, url: *mut *const c_char, keep: *mut bool) -> i32;
}

struct Rng(Cell<u64>);
impl Rng {
    fn next(&self) -> u64 {
        let mut x = self.0.get();
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.0.set(x);
        x.wrapping_mul(0x2545_f491_4f6c_dd1d)
    }
    fn pick<T: Copy>(&self, items: &[T]) -> T {
        items[(self.next() % items.len() as u64) as usize]
    }
}

fn url(rng: &Rng) -> String {
    let mut u = rng
        .pick(&[
            "https://github.com/shufps/",
            "https://github.com/shufps/nerd-os-rs/releases/download/",
            "HTTPS://GitHub.com/shufps/NERD-OS-rs/releases/download/",
            "HTTPS://GitHub.com/shufps/",
            "https://github.com/shufpsx/",
            "https://github.com/shufps",
            "http://github.com/shufps/",
            "https://github.com.evil/shufps/",
            "",
        ])
        .to_string();
    for _ in 0..(rng.next() % 6) {
        u.push_str(rng.pick(&[
            "ESP-Miner-NerdQAxePlus",
            "/",
            "releases/download/v1.4.0/",
            "factory.bin",
            "..",
            ".",
            "%2e",
            "%2E",
            "%2e%2e",
            "%2E%2E",
            "%2E%2e",
            "%2e%2E",
            "%2f",
            "%5c",
            "\\",
            "\t",
            "\x7f",
            "?x=1",
            "#f",
            "%",
            "%2",
            "a..b",
            "@evil.com",
            "%25",
        ]));
    }
    u
}

/// The new check rejects at least what the old one did; it accepts less
/// where percent-encoding is involved and outside the Rust
/// firmware's release downloads (the old check took all of `shufps/`).
const RS: &str = "https://github.com/shufps/nerd-os-rs/releases/download/";

#[test]
fn factory_url_check_is_at_least_as_strict() {
    let rng = Rng(Cell::new(0x0a7a));
    let mut stricter = 0;
    let mut accepted = 0;
    for i in 0..200_000 {
        let u = url(&rng);
        let c = CString::new(u.clone()).unwrap();
        let old = unsafe { ref_factory_url_ok(c.as_ptr()) };
        let new = factory_url_ok(u.as_bytes());
        accepted += new as u32;
        if new != old {
            let rs_release = u.len() >= RS.len() && u[..RS.len()].eq_ignore_ascii_case(RS);
            assert!(
                old && !new && (u.contains('%') || !rs_release),
                "case {i}: {u:?} old {old} new {new}"
            );
            stricter += 1;
        }
    }
    assert!(stricter > 0, "the encoded traversal cases were generated");
    assert!(accepted > 1000, "valid release URLs were generated");
}

#[test]
fn factory_bodies_match_reference() {
    let rng = Rng(Cell::new(0xb0d7));
    let values = [
        "\"https://github.com/shufps/x\"",
        "\"\"",
        "1",
        "null",
        "true",
        "false",
        "'u'",
        "\"a\\u0000b\"",
        "[]",
        "{}",
        "\"0\"",
        "0",
    ];
    for i in 0..30_000 {
        let n = rng.next() % 4;
        let members: Vec<String> = (0..n)
            .map(|_| {
                format!(
                    "\"{}\":{}",
                    rng.pick(&["url", "keep_config", "x", "URL"]),
                    rng.pick(&values)
                )
            })
            .collect();
        let mut b = format!("{{{}}}", members.join(","));
        if rng.next() % 8 == 0 {
            b.truncate((rng.next() % (b.len() as u64 + 1)) as usize);
        }
        let c = CString::new(b.clone()).unwrap();
        let (mut url, mut keep) = (std::ptr::null(), false);
        let r = unsafe { ref_factory_body(c.as_ptr(), &mut url, &mut keep) };
        let reference = match r {
            1 => FactoryBody::Invalid,
            2 => FactoryBody::NoUrl,
            _ => FactoryBody::Ok {
                url: unsafe { CStr::from_ptr(url) }.to_bytes().to_vec(),
                keep_config: keep,
            },
        };
        assert_eq!(factory_body(b.as_bytes()), reference, "case {i}: {b}");
    }
}
