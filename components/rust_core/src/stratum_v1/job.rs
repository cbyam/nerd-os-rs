//! Job construction for Stratum v1 (port of `MiningInfoV1::buildBmJob` in
//! `main/tasks/create_jobs_task.cpp` and `construct_bm_job()`): extranonce2,
//! coinbase transaction, merkle root and the job's hash fields.

use core::ffi::{c_char, c_int};
use std::io::Write;

use super::{MiningNotify, EXTRANONCE2_LEN};
use crate::cutil::{hex2bin, until_nul};
use crate::mining::{merkle_root, JobHashes};

/// Longest extranonce2 hex string plus NUL.
pub const EXTRANONCE2_HEX_MAX: usize = 2 * 16 + 1;

/// Hash fields and extranonce2 of a job, `stratum_v1_job_t`.
#[repr(C)]
pub struct Job {
    pub hashes: JobHashes,
    /// Hex, NUL-terminated; also sent with `mining.submit`.
    pub extranonce2: [c_char; EXTRANONCE2_HEX_MAX],
}

/// extranonce2 as `len` bytes of hex: the low `len` bytes of the counter,
/// zero-padded (a counter that outgrows `len` keeps its *low* digits).
pub fn extranonce2_hex(counter: u32, len: usize) -> String {
    let digits = 2 * len;
    let full = format!("{counter:0digits$x}");
    full[full.len() - digits..].to_string()
}

/// Builds the job's hash fields for the notify, extranonce1 (hex) and the
/// extranonce2 counter. None if `extranonce2_len` is outside 1..=16.
pub fn build(
    notify: &MiningNotify,
    coinbase: [&[u8]; 3],
    extranonce2_len: i32,
    extranonce2: u32,
) -> Option<(JobHashes, String)> {
    let n = notify.n_merkle_branches.min(notify.merkle_branches.len());
    build_parts(
        &notify.prev_block_hash,
        &notify.merkle_branches[..n],
        coinbase,
        extranonce2_len,
        extranonce2,
    )
}

/// [`build`] from the notify's prevhash and merkle branches.
pub fn build_parts(
    prev_block_hash: &[u8; 32],
    merkle_branches: &[[u8; 32]],
    coinbase: [&[u8]; 3],
    extranonce2_len: i32,
    extranonce2: u32,
) -> Option<(JobHashes, String)> {
    if !EXTRANONCE2_LEN.contains(&extranonce2_len) {
        return None;
    }
    let extranonce2 = extranonce2_hex(extranonce2, extranonce2_len as usize);
    let [coinbase_1, extranonce1, coinbase_2] = coinbase;

    let mut tx_hex = Vec::with_capacity(
        coinbase_1.len() + extranonce1.len() + extranonce2.len() + coinbase_2.len(),
    );
    for part in [coinbase_1, extranonce1, extranonce2.as_bytes(), coinbase_2] {
        let _ = tx_hex.write_all(part);
    }
    let mut tx = vec![0u8; tx_hex.len() / 2];
    hex2bin(&tx_hex, &mut tx);

    let root = merkle_root(&tx, merkle_branches);
    Some((JobHashes::new(root, prev_block_hash), extranonce2))
}

mod ffi {
    use super::*;
    use core::ffi::CStr;

    unsafe fn bytes<'a>(p: *const c_char) -> Option<&'a [u8]> {
        (!p.is_null()).then(|| until_nul(CStr::from_ptr(p).to_bytes()))
    }

    /// Fills `*job` for the current notify; false for missing strings or an
    /// `extranonce2_len` outside 1..=16.
    ///
    /// # Safety
    /// `notify` and `job` must be valid; strings NUL-terminated or NULL.
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_build_job(
        notify: *const MiningNotify,
        extranonce1: *const c_char,
        extranonce2_len: c_int,
        extranonce2: u32,
        job: *mut Job,
    ) -> bool {
        if notify.is_null() || job.is_null() {
            return false;
        }
        let notify = &*notify;
        let (Some(cb1), Some(en1), Some(cb2)) = (
            bytes(notify.coinbase_1),
            bytes(extranonce1),
            bytes(notify.coinbase_2),
        ) else {
            return false;
        };
        let Some((hashes, en2)) = build(notify, [cb1, en1, cb2], extranonce2_len, extranonce2)
        else {
            return false;
        };
        let job = &mut *job;
        job.hashes = hashes;
        job.extranonce2 = [0; EXTRANONCE2_HEX_MAX];
        for (dst, &b) in job.extranonce2.iter_mut().zip(en2.as_bytes()) {
            *dst = b as c_char;
        }
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extranonce2_formatting() {
        assert_eq!(extranonce2_hex(0x1234, 4), "00001234");
        assert_eq!(extranonce2_hex(0xdeadbeef, 8), "00000000deadbeef");
        assert_eq!(extranonce2_hex(0x1234, 1), "34");
        assert_eq!(extranonce2_hex(0x0203_0001, 2), "0001");
    }
}
