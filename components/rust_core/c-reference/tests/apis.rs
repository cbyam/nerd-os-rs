//! Differential test of the mempool API responses: the former parsing
//! (c-http/apis_ref.cpp, verbatim, with ArduinoJson) against rust_core::apis.

use std::cell::Cell;
use std::ffi::{c_char, CString};

use c_reference as _;
use rust_core::apis::{parse, Api, Values};

extern "C" {
    fn ref_apis_parse(api: i32, body: *const c_char, values: *mut Values) -> bool;
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

const NUMBERS: &[&str] = &[
    "0",
    "1",
    "97123",
    "-5",
    "4294967295",
    "4294967296",
    "1.9",
    "873456",
    "7.5e20",
    "6.12e20",
    "1.1e14",
    "1.2345678901e14",
    "9.9e17",
    "\"123\"",
    "\"12.5\"",
    "\"x\"",
    "true",
    "null",
    "[]",
    "{}",
    "1e3",
    "-0",
    "123456789012345678901234567890",
    "1.8e19",
    "1.8e31",
];

#[test]
fn responses_match_reference() {
    let rng = Rng(Cell::new(0xa915));
    let keys: [&[&str]; 4] = [
        &["USD", "EUR", "time"],
        &[],
        &["currentHashrate", "currentDifficulty", "hashrates"],
        &["hourFee", "halfHourFee", "fastestFee", "economyFee"],
    ];
    let mut compared = 0;
    for i in 0..40_000 {
        let api = (rng.next() % 4) as i32;
        let body = if api == 1 || rng.next() % 10 == 0 {
            rng.pick(NUMBERS).to_string()
        } else {
            let members: Vec<String> = (0..rng.next() % 5)
                .map(|_| {
                    let key = if rng.next() % 6 == 0 {
                        "x"
                    } else {
                        rng.pick(keys[api as usize])
                    };
                    format!("\"{key}\":{}", rng.pick(NUMBERS))
                })
                .collect();
            format!("{{{}}}", members.join(","))
        };
        // hashrate/difficulty beyond uint64_t or negative: undefined in C
        if api == 2 && (body.contains('-') || body.contains("e31") || body.contains("890")) {
            continue;
        }
        let c = CString::new(body.clone()).unwrap();
        let mut r = Values::default();
        let ok_ref = unsafe { ref_apis_parse(api, c.as_ptr(), &mut r) };
        let mut n = Values::default();
        let ok_new = parse(
            [Api::Price, Api::BlockHeight, Api::Hashrate, Api::Fees][api as usize],
            body.as_bytes(),
            &mut n,
        );
        assert_eq!((ok_new, n), (ok_ref, r), "case {i} api {api}: {body}");
        compared += 1;
    }
    assert!(compared > 30_000);
}
