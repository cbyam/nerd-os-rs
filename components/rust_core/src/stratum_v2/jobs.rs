//! Job construction for Stratum v2 (port of `MiningInfoV2Standard/Extended::
//! buildBmJob` in `main/tasks/mining_info_v2.cpp`): the bm_job hash fields
//! from SV2 data, which is in internal byte order, and for extended channels
//! the coinbase transaction and merkle root.

use crate::mining::{merkle_root, swap_words, JobHashes};

/// Most extranonce bytes an extended channel can have (B0_32 in the spec).
pub const MAX_EXTRANONCE: usize = 32;

/// Hash fields from a merkle root and prevhash in internal byte order. SV2
/// sends the prevhash unswapped; `JobHashes::new` expects it as `mining.notify`
/// sends it (word-swapped), so swap first.
pub fn hashes(merkle_root: [u8; 32], prev_hash: &[u8; 32]) -> JobHashes {
    JobHashes::new(merkle_root, &swap_words(prev_hash))
}

/// extranonce2 for the counter: `size` bytes, big-endian, the counter's low
/// bytes at the end (zero padded; for `size` < 4 the high bytes are dropped).
pub fn extranonce2(counter: u32, size: usize) -> Vec<u8> {
    let be = counter.to_be_bytes();
    let mut out = vec![0u8; size];
    let n = size.min(4);
    out[size - n..].copy_from_slice(&be[4 - n..]);
    out
}

/// Extended channel: coinbase = prefix | extranonce prefix | extranonce2 |
/// suffix, merkle root over the merkle path. None if the extranonce does not
/// fit (`extranonce_size` > 32).
pub struct ExtendedJob<'a> {
    pub coinbase_prefix: &'a [u8],
    pub extranonce_prefix: &'a [u8],
    pub extranonce_size: usize,
    pub coinbase_suffix: &'a [u8],
    pub merkle_path: &'a [[u8; 32]],
    pub prev_hash: &'a [u8; 32],
}

impl ExtendedJob<'_> {
    /// Hash fields and extranonce2 (sent with the share) for `counter`.
    pub fn build(&self, counter: u32) -> Option<(JobHashes, Vec<u8>)> {
        if self.extranonce_size > MAX_EXTRANONCE {
            return None;
        }
        let en2 = extranonce2(counter, self.extranonce_size);
        let coinbase = [
            self.coinbase_prefix,
            self.extranonce_prefix,
            &en2,
            self.coinbase_suffix,
        ]
        .concat();
        let root = merkle_root(&coinbase, self.merkle_path);
        Some((hashes(root, self.prev_hash), en2))
    }
}

mod ffi {
    use super::*;
    use core::ffi::c_char;

    unsafe fn slice<'a, T>(p: *const T, len: usize) -> &'a [T] {
        if p.is_null() || len == 0 {
            &[]
        } else {
            core::slice::from_raw_parts(p, len)
        }
    }

    /// Standard channel: hash fields from the pool's merkle root and prevhash.
    ///
    /// # Safety
    /// Pointers to 32 bytes each, `out` writable.
    #[no_mangle]
    pub unsafe extern "C" fn sv2_job_hashes(
        merkle_root: *const [u8; 32],
        prev_hash: *const [u8; 32],
        out: *mut JobHashes,
    ) {
        *out = hashes(*merkle_root, &*prev_hash);
    }

    /// Extended channel: hash fields and extranonce2 (as hex, NUL-terminated,
    /// into `extranonce2_hex`, 65 bytes) for the counter. false if
    /// `extranonce_size` is larger than 32.
    ///
    /// # Safety
    /// Pointers valid for the given lengths; `merkle_path` holds
    /// `merkle_path_count` hashes; `extranonce2_hex` 65 bytes.
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_build_extended_job(
        coinbase_prefix: *const u8,
        coinbase_prefix_len: u16,
        extranonce_prefix: *const u8,
        extranonce_prefix_len: u8,
        extranonce_size: u8,
        coinbase_suffix: *const u8,
        coinbase_suffix_len: u16,
        merkle_path: *const [u8; 32],
        merkle_path_count: u8,
        prev_hash: *const [u8; 32],
        extranonce2_counter: u32,
        out: *mut JobHashes,
        extranonce2_hex: *mut c_char,
    ) -> bool {
        let job = ExtendedJob {
            coinbase_prefix: slice(coinbase_prefix, coinbase_prefix_len as usize),
            extranonce_prefix: slice(extranonce_prefix, extranonce_prefix_len as usize),
            extranonce_size: extranonce_size as usize,
            coinbase_suffix: slice(coinbase_suffix, coinbase_suffix_len as usize),
            merkle_path: slice(merkle_path, merkle_path_count as usize),
            prev_hash: &*prev_hash,
        };
        let Some((hashes, en2)) = job.build(extranonce2_counter) else {
            return false;
        };
        *out = hashes;
        let hex = crate::cutil::to_hex(&en2);
        core::ptr::copy_nonoverlapping(hex.as_ptr(), extranonce2_hex as *mut u8, hex.len());
        *extranonce2_hex.add(hex.len()) = 0;
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extranonce2_layout() {
        assert_eq!(extranonce2(0x0102_0304, 6), [0, 0, 1, 2, 3, 4]);
        assert_eq!(extranonce2(0x0102_0304, 2), [3, 4]);
        assert_eq!(extranonce2(7, 0), Vec::<u8>::new());
    }

    #[test]
    fn rejects_oversized_extranonce() {
        let job = ExtendedJob {
            coinbase_prefix: b"",
            extranonce_prefix: b"",
            extranonce_size: 33,
            coinbase_suffix: b"",
            merkle_path: &[],
            prev_hash: &[0; 32],
        };
        assert!(job.build(1).is_none());
    }
}
