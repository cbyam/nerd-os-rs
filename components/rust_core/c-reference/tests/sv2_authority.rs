//! Differential tests of the SV2 authority key decoding:
//! rust_core::stratum_v2::authority against the former
//! loadAuthorityPubkey() with libbase58 (c-sv2key/, verbatim): valid keys
//! give the same 32 bytes; everything the former code refused is refused;
//! the only keys accepted before and refused now have a wrong checksum.

use std::ffi::{c_char, CString};

use c_reference as _;
use rust_core::mining::sha256d;
use rust_core::stratum_v2::authority::{authority_key, KeyError};

extern "C" {
    fn ref_sv2_authority_key(b58: *const c_char, out: *mut u8) -> bool;
}

struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }
}

const ALPHABET: &[u8] = b"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/// base58 (big number division, leading zero bytes as '1')
fn b58(data: &[u8]) -> String {
    let mut digits: Vec<u8> = Vec::new();
    for &byte in data {
        let mut carry = byte as u32;
        for d in digits.iter_mut() {
            carry += (*d as u32) << 8;
            *d = (carry % 58) as u8;
            carry /= 58;
        }
        while carry > 0 {
            digits.push((carry % 58) as u8);
            carry /= 58;
        }
    }
    let zeros = data.iter().take_while(|&&b| b == 0).count();
    std::iter::repeat(b'1')
        .take(zeros)
        .chain(digits.iter().rev().map(|&d| ALPHABET[d as usize]))
        .map(char::from)
        .collect()
}

fn reference(s: &str) -> Option<[u8; 32]> {
    let c = CString::new(s).unwrap();
    let mut out = [0u8; 32];
    unsafe { ref_sv2_authority_key(c.as_ptr(), out.as_mut_ptr()) }.then_some(out)
}

fn check(s: &str) {
    let old = reference(s);
    let new = authority_key(s);
    match (old, new) {
        (Some(a), Ok(b)) => assert_eq!(a, b, "{s}"),
        (None, Err(_)) => {}
        (Some(_), Err(e)) => assert_eq!(e, KeyError::Checksum, "{s}"),
        (None, Ok(_)) => panic!("accepted now, refused before: {s}"),
    }
}

#[test]
fn keys_match_the_former_decoder() {
    let mut rng = Rng(0x5eed_00b5_8000_0001);
    for i in 0..20_000 {
        let mut p: Vec<u8> = Vec::new();
        let version = if i % 5 == 0 {
            (rng.next() % 4) as u16
        } else {
            1
        };
        p.extend_from_slice(&version.to_le_bytes());
        // also keys with leading zero bytes
        for j in 0..32 {
            p.push(if i % 7 == 0 && j < 3 {
                0
            } else {
                rng.next() as u8
            });
        }
        let mut c = sha256d(&p)[..4].to_vec();
        if i % 3 == 0 {
            c[(rng.next() % 4) as usize] ^= 1 << (rng.next() % 8);
        }
        p.extend_from_slice(&c);
        let s = b58(&p);
        check(&s);
        // a typo
        let mut t: Vec<char> = s.chars().collect();
        let at = (rng.next() as usize) % t.len();
        t[at] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz".as_bytes()
            [(rng.next() % 58) as usize] as char;
        check(&t.into_iter().collect::<String>());
        // other lengths
        let n = (rng.next() % 60) as usize;
        let r: Vec<u8> = (0..n).map(|_| rng.next() as u8).collect();
        check(&b58(&r));
    }
    for s in [
        "",
        "0",
        "O",
        "l",
        "I",
        " 9bCo",
        "9bCo ",
        "abc\u{e4}",
        "1111111111111111111111111111111111111111111111111111",
    ] {
        check(s);
    }
}
