//! Differential tests of the JSON serializer: ajson::Json::serialize against
//! ArduinoJson's serializeJson() (cpp-shim/json_write_ref.cpp), for floats,
//! doubles and whole documents.

use std::ffi::c_char;

use c_reference as _;
use rust_core::ajson::{deserialize, Json, Num};

extern "C" {
    fn ref_json_number(v: f64, is_float: bool, out: *mut c_char, cap: usize) -> usize;
    fn ref_json_roundtrip(input: *const c_char, len: usize, out: *mut c_char, cap: usize) -> usize;
}

fn reference_number(v: f64, is_float: bool) -> String {
    let mut buf = vec![0u8; 128];
    let n = unsafe { ref_json_number(v, is_float, buf.as_mut_ptr() as *mut c_char, buf.len()) };
    String::from_utf8_lossy(&buf[..n]).into_owned()
}

fn ours(v: Num) -> String {
    String::from_utf8(Json::Num(v).serialize()).unwrap()
}

/// xorshift64
struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }
}

fn check_double(v: f64) {
    assert_eq!(
        ours(Num::double(v)),
        reference_number(v, false),
        "double {v:e} ({:#x})",
        v.to_bits()
    );
}

fn check_float(v: f32) {
    assert_eq!(
        ours(Num::F(v)),
        reference_number(v as f64, true),
        "float {v:e} ({:#x})",
        v.to_bits()
    );
}

#[test]
fn special_and_typical_numbers() {
    let values = [
        0.0,
        -0.0,
        1.0,
        -1.0,
        0.5,
        0.1,
        0.05,
        1e-5,
        1.0000001e-5,
        9.99999e-6,
        1e7,
        9999999.0,
        9999999.5,
        10000000.5,
        1e300,
        1e-300,
        5e-324,
        f64::MAX,
        f64::MIN_POSITIVE,
        11684.98,
        11699.121094,
        2905.0,
        134823837300000.0,
        3907780421.0,
        3907780421.5,
        0.171342,
        60.3042,
        123456789.12345679,
        1.5e-7,
        f64::NAN,
        f64::INFINITY,
        f64::NEG_INFINITY,
        9.9999999995,
        99.9999999995,
        0.9999999995,
    ];
    for v in values {
        check_double(v);
        check_float(v as f32);
    }
}

#[test]
fn random_doubles_and_floats() {
    let mut r = Rng(0x9e3779b97f4a7c15);
    for _ in 0..200_000 {
        let bits = r.next();
        // all bit patterns, and values of a "normal" magnitude
        check_double(f64::from_bits(bits));
        check_float(f32::from_bits(bits as u32));
        let scaled =
            (bits >> 11) as f64 / (1u64 << 53) as f64 * 10f64.powi((r.next() % 24) as i32 - 8);
        check_double(scaled);
        check_float(scaled as f32);
        // values with few decimals, like rates and temperatures
        let d = (r.next() % 100_000_000) as f64 / 10f64.powi((r.next() % 7) as i32);
        check_double(d);
        check_float(d as f32);
    }
}

fn roundtrip(input: &[u8]) {
    let mut buf = vec![0u8; 4096];
    let n = unsafe {
        ref_json_roundtrip(
            input.as_ptr() as *const c_char,
            input.len(),
            buf.as_mut_ptr() as *mut c_char,
            buf.len(),
        )
    };
    match deserialize(input) {
        Ok(doc) => {
            assert_ne!(
                n,
                usize::MAX,
                "ArduinoJson rejects {:?}",
                String::from_utf8_lossy(input)
            );
            assert_eq!(
                String::from_utf8_lossy(&doc.serialize()),
                String::from_utf8_lossy(&buf[..n]),
                "input {:?}",
                String::from_utf8_lossy(input)
            );
        }
        Err(_) => assert_eq!(
            n,
            usize::MAX,
            "we reject {:?}",
            String::from_utf8_lossy(input)
        ),
    }
}

#[test]
fn documents() {
    for doc in [
        &b"{\"a\":1,\"b\":[true,false,null],\"c\":{\"d\":\"x\\\"y\\\\z\\n\\t\\b\\f\\r\\u0001\\u0000/\"}}"[..],
        b"[1.5,-2,3e10,0.1,-0.0,18446744073709551615,-9223372036854775808,1e400]",
        b"{'single':'quotes',unquoted:01}",
        b"{\"utf8\":\"\xc3\xa4\\u00e4\\ud83d\\ude00\"}",
        b"{\"dup\":1,\"x\":2,\"dup\":3}",
        b"{\"pidP\":6.0,\"pidI\":0.1,\"hashRate\":11684.98046875,\"temp\":60.304}",
        b"[]",
        b"{}",
        b"\"top\"",
        b"12.75",
    ] {
        roundtrip(doc);
    }
}

#[test]
fn random_documents() {
    let mut r = Rng(0x1234_5678_9abc_def1);
    let atoms: [&[u8]; 14] = [
        b"1",
        b"-1",
        b"0.25",
        b"1e-7",
        b"123456789.5",
        b"true",
        b"null",
        b"\"s\"",
        b"\"\\u0007\"",
        b"\"a\\\"b\"",
        b"4294967296",
        b"-0.0001",
        b"3.14159265358979",
        b"1e21",
    ];
    for _ in 0..20_000 {
        let mut s = Vec::new();
        gen(&mut r, &atoms, &mut s, 3);
        roundtrip(&s);
    }
}

fn gen(r: &mut Rng, atoms: &[&[u8]], out: &mut Vec<u8>, depth: u32) {
    match r.next() % if depth == 0 { 1 } else { 3 } {
        0 => out.extend_from_slice(atoms[(r.next() % atoms.len() as u64) as usize]),
        1 => {
            out.push(b'[');
            for i in 0..r.next() % 4 {
                if i > 0 {
                    out.push(b',');
                }
                gen(r, atoms, out, depth - 1);
            }
            out.push(b']');
        }
        _ => {
            out.push(b'{');
            for i in 0..r.next() % 4 {
                if i > 0 {
                    out.push(b',');
                }
                out.extend_from_slice(format!("\"k{}\":", r.next() % 3).as_bytes());
                gen(r, atoms, out, depth - 1);
            }
            out.push(b'}');
        }
    }
}
