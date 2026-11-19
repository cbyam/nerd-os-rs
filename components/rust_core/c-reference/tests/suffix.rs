//! suffixString(): rust_core::suffix against the former C++ (c-utils/,
//! verbatim): the same text for values around every suffix boundary and
//! random values, with 0 (three significant digits) and 1..5 digits.

use std::ffi::{c_char, c_int, CStr};

use c_reference as _;
use rust_core::suffix::suffix_string;

extern "C" {
    fn ref_suffixString(val: u64, buf: *mut c_char, size: usize, sig: c_int);
}

fn reference(v: u64, sig: i32) -> String {
    let mut b = [0 as c_char; 64];
    unsafe { ref_suffixString(v, b.as_mut_ptr(), b.len(), sig) };
    unsafe { CStr::from_ptr(b.as_ptr()) }
        .to_str()
        .unwrap()
        .to_owned()
}

#[test]
fn matches_the_former_code() {
    let mut values = vec![0u64, 1, 9, 999, u64::MAX, u64::MAX - 1];
    let mut p = 1u64;
    for _ in 0..19 {
        p = p.saturating_mul(10);
        for d in [-2i64, -1, 0, 1, 2, 499, 500, 501, 4999, 5000, 5001] {
            values.push((p as i64).saturating_add(d) as u64);
            values.push(p.saturating_mul(9).saturating_add(d as u64));
        }
    }
    let mut x = 0x9e37_79b9_7f4a_7c15u64;
    for _ in 0..50_000 {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        values.push(x >> (x % 64));
    }
    for v in values {
        for sig in [0, 1, 2, 3, 4, 5] {
            assert_eq!(suffix_string(v, sig), reference(v, sig), "{v} sig {sig}");
        }
    }
}
