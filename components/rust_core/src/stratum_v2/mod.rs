//! Stratum v2: the binary codec ([`protocol`]), the Noise transport
//! ([`noise`]), the session state ([`session`]) and job construction
//! ([`jobs`]).

pub mod authority;
pub mod connection;
pub mod jobs;
pub mod noise;
pub mod protocol;
pub mod session;

use core::ffi::{c_char, c_int, c_void};

use protocol::{FrameHeader, MAX_MERKLE_BRANCHES};

/// `sv2_ext_job_t`: allocated by `sv2_parse_new_extended_mining_job()`,
/// released with `sv2_ext_job_free()`. The C side fills in the extranonce
/// prefix and may read every field.
#[repr(C)]
pub struct ExtJob {
    pub job_id: u32,
    pub version: u32,
    pub version_rolling_allowed: bool,
    pub prev_hash: [u8; 32],
    pub ntime: u32,
    pub nbits: u32,
    pub clean_jobs: bool,
    pub merkle_path: [[u8; 32]; MAX_MERKLE_BRANCHES],
    pub merkle_path_count: u8,
    pub coinbase_prefix: *mut u8,
    pub coinbase_prefix_len: u16,
    pub coinbase_suffix: *mut u8,
    pub coinbase_suffix_len: u16,
    pub extranonce_prefix: [u8; 32],
    pub extranonce_prefix_len: u8,
}

// Must match the static assertions in sv2_protocol.h.
#[cfg(target_pointer_width = "32")]
const _: () = assert!(core::mem::size_of::<ExtJob>() == 744);
#[cfg(target_pointer_width = "64")]
const _: () = assert!(core::mem::size_of::<ExtJob>() == 760);
const _: () = assert!(core::mem::size_of::<FrameHeader>() == 8);

extern "C" {
    fn free(p: *mut c_void);
}

/// A `malloc()`ed copy of `data` (NULL for empty data); None if out of memory.
unsafe fn c_copy(data: &[u8]) -> Option<*mut u8> {
    if data.is_empty() {
        return Some(core::ptr::null_mut());
    }
    let p = crate::cutil::c_malloc(data.len()) as *mut u8;
    if p.is_null() {
        return None;
    }
    core::ptr::copy_nonoverlapping(data.as_ptr(), p, data.len());
    Some(p)
}

/// Allocates an `sv2_ext_job_t` (released with `sv2_ext_job_free()`); NULL if
/// out of memory.
#[allow(clippy::too_many_arguments)] // one per field
pub(crate) unsafe fn alloc_ext_job(
    job_id: u32,
    version: u32,
    prev_hash: [u8; 32],
    ntime: u32,
    nbits: u32,
    clean: bool,
    merkle_path: &[[u8; 32]],
    coinbase_prefix: &[u8],
    coinbase_suffix: &[u8],
    extranonce_prefix: &[u8],
) -> *mut ExtJob {
    let job = crate::cutil::c_alloc_zeroed::<ExtJob>();
    if job.is_null() {
        return job;
    }
    let out = &mut *job;
    out.job_id = job_id;
    out.version = version;
    // Jobs reaching C were only accepted with version rolling allowed.
    out.version_rolling_allowed = true;
    out.prev_hash = prev_hash;
    out.ntime = ntime;
    out.nbits = nbits;
    out.clean_jobs = clean;
    let n = merkle_path.len().min(MAX_MERKLE_BRANCHES);
    out.merkle_path_count = n as u8;
    out.merkle_path[..n].copy_from_slice(&merkle_path[..n]);
    out.coinbase_prefix_len = coinbase_prefix.len() as u16;
    out.coinbase_suffix_len = coinbase_suffix.len() as u16;
    let n = extranonce_prefix.len().min(32);
    out.extranonce_prefix[..n].copy_from_slice(&extranonce_prefix[..n]);
    out.extranonce_prefix_len = n as u8;
    match (c_copy(coinbase_prefix), c_copy(coinbase_suffix)) {
        (Some(prefix), Some(suffix)) => {
            out.coinbase_prefix = prefix;
            out.coinbase_suffix = suffix;
            job
        }
        (prefix, suffix) => {
            free(prefix.unwrap_or(core::ptr::null_mut()) as _);
            free(suffix.unwrap_or(core::ptr::null_mut()) as _);
            free(job as _);
            core::ptr::null_mut()
        }
    }
}

pub(crate) mod ffi {
    use super::protocol::*;
    use super::*;
    use core::ffi::CStr;

    unsafe fn slice<'a>(p: *const u8, len: u32) -> &'a [u8] {
        if p.is_null() || len == 0 {
            &[]
        } else {
            core::slice::from_raw_parts(p, len as usize)
        }
    }

    unsafe fn cstr<'a>(p: *const c_char) -> &'a [u8] {
        if p.is_null() {
            &[]
        } else {
            CStr::from_ptr(p).to_bytes()
        }
    }

    /// Copies a frame into `buf`; its length, or -1 if it does not fit.
    unsafe fn emit(buf: *mut u8, buf_len: usize, frame: Option<Vec<u8>>) -> c_int {
        match frame {
            Some(f) if !buf.is_null() && f.len() <= buf_len => {
                core::ptr::copy_nonoverlapping(f.as_ptr(), buf, f.len());
                f.len() as c_int
            }
            _ => -1,
        }
    }

    unsafe fn copy_prefix(src: &[u8], dst: *mut u8, dst_len: *mut u8) {
        if !src.is_empty() {
            core::ptr::copy_nonoverlapping(src.as_ptr(), dst, src.len());
        }
        *dst_len = src.len() as u8;
    }

    // --- frame header ---

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_frame_header(
        data: *const [u8; 6],
        header: *mut FrameHeader,
    ) -> c_int {
        *header = parse_frame_header(&*data);
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_encode_frame_header(
        dest: *mut [u8; 6],
        extension_type: u16,
        msg_type: u8,
        msg_length: u32,
    ) -> c_int {
        *dest = encode_frame_header(extension_type, msg_type, msg_length);
        FRAME_HEADER_SIZE as c_int
    }

    // --- builders ---

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_build_setup_connection(
        buf: *mut u8,
        buf_len: usize,
        host: *const c_char,
        port: u16,
        vendor: *const c_char,
        hw_version: *const c_char,
        firmware: *const c_char,
        device_id: *const c_char,
        flags: u32,
    ) -> c_int {
        let frame = setup_connection(
            cstr(host),
            port,
            cstr(vendor),
            cstr(hw_version),
            cstr(firmware),
            cstr(device_id),
            flags,
        );
        emit(buf, buf_len, frame)
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_build_open_standard_mining_channel(
        buf: *mut u8,
        buf_len: usize,
        request_id: u32,
        user_identity: *const c_char,
        nominal_hash_rate: f32,
    ) -> c_int {
        let frame =
            open_standard_mining_channel(request_id, cstr(user_identity), nominal_hash_rate);
        emit(buf, buf_len, Some(frame))
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_build_open_extended_mining_channel(
        buf: *mut u8,
        buf_len: usize,
        request_id: u32,
        user_identity: *const c_char,
        nominal_hash_rate: f32,
        min_extranonce_size: u16,
    ) -> c_int {
        let frame = open_extended_mining_channel(
            request_id,
            cstr(user_identity),
            nominal_hash_rate,
            min_extranonce_size,
        );
        emit(buf, buf_len, Some(frame))
    }

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_build_submit_shares_standard(
        buf: *mut u8,
        buf_len: usize,
        channel_id: u32,
        sequence_number: u32,
        job_id: u32,
        nonce: u32,
        ntime: u32,
        version: u32,
    ) -> c_int {
        let frame =
            submit_shares_standard(channel_id, sequence_number, job_id, nonce, ntime, version);
        emit(buf, buf_len, Some(frame))
    }

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_build_submit_shares_extended(
        buf: *mut u8,
        buf_len: usize,
        channel_id: u32,
        sequence_number: u32,
        job_id: u32,
        nonce: u32,
        ntime: u32,
        version: u32,
        extranonce: *const u8,
        extranonce_len: u8,
    ) -> c_int {
        let frame = submit_shares_extended(
            channel_id,
            sequence_number,
            job_id,
            nonce,
            ntime,
            version,
            slice(extranonce, extranonce_len as u32),
        );
        emit(buf, buf_len, Some(frame))
    }

    // --- parsers: 0 on success, -1 on error ---

    fn status(ok: bool) -> c_int {
        if ok {
            0
        } else {
            -1
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_setup_connection_success(
        payload: *const u8,
        len: u32,
        used_version: *mut u16,
        flags: *mut u32,
    ) -> c_int {
        let parsed = setup_connection_success(slice(payload, len));
        if let Some((v, f)) = parsed {
            (*used_version, *flags) = (v, f);
        }
        status(parsed.is_some())
    }

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_parse_open_channel_success(
        payload: *const u8,
        len: u32,
        request_id: *mut u32,
        channel_id: *mut u32,
        target: *mut [u8; 32],
        extranonce_prefix: *mut u8,
        extranonce_prefix_len: *mut u8,
        group_channel_id: *mut u32,
    ) -> c_int {
        let Some(s) = open_channel_success(slice(payload, len), false) else {
            return -1;
        };
        (*request_id, *channel_id, *target, *group_channel_id) =
            (s.request_id, s.channel_id, s.target, s.group_channel_id);
        copy_prefix(
            s.extranonce_prefix,
            extranonce_prefix,
            extranonce_prefix_len,
        );
        0
    }

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_parse_open_extended_channel_success(
        payload: *const u8,
        len: u32,
        request_id: *mut u32,
        channel_id: *mut u32,
        target: *mut [u8; 32],
        extranonce_size: *mut u16,
        extranonce_prefix: *mut u8,
        extranonce_prefix_len: *mut u8,
        group_channel_id: *mut u32,
    ) -> c_int {
        let Some(s) = open_channel_success(slice(payload, len), true) else {
            return -1;
        };
        (*request_id, *channel_id, *target, *group_channel_id) =
            (s.request_id, s.channel_id, s.target, s.group_channel_id);
        *extranonce_size = s.extranonce_size;
        copy_prefix(
            s.extranonce_prefix,
            extranonce_prefix,
            extranonce_prefix_len,
        );
        0
    }

    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn sv2_parse_new_mining_job(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        job_id: *mut u32,
        has_min_ntime: *mut bool,
        min_ntime: *mut u32,
        version: *mut u32,
        merkle_root: *mut [u8; 32],
    ) -> c_int {
        let Some(j) = new_mining_job(slice(payload, len)) else {
            return -1;
        };
        (*channel_id, *job_id, *version, *merkle_root) =
            (j.channel_id, j.job_id, j.version, j.merkle_root);
        *has_min_ntime = j.min_ntime.is_some();
        *min_ntime = j.min_ntime.unwrap_or(0);
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_set_new_prev_hash(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        job_id: *mut u32,
        prev_hash: *mut [u8; 32],
        min_ntime: *mut u32,
        nbits: *mut u32,
    ) -> c_int {
        let Some(p) = set_new_prev_hash(slice(payload, len)) else {
            return -1;
        };
        (*channel_id, *job_id, *prev_hash, *min_ntime, *nbits) = p;
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_set_extranonce_prefix(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        extranonce_prefix: *mut u8,
        extranonce_prefix_len: *mut u8,
    ) -> c_int {
        let Some((id, prefix)) = set_extranonce_prefix(slice(payload, len)) else {
            return -1;
        };
        *channel_id = id;
        copy_prefix(prefix, extranonce_prefix, extranonce_prefix_len);
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_set_target(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        max_target: *mut [u8; 32],
    ) -> c_int {
        let Some(p) = set_target(slice(payload, len)) else {
            return -1;
        };
        (*channel_id, *max_target) = p;
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_submit_shares_success(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        new_submits_accepted_count: *mut u32,
    ) -> c_int {
        let Some(p) = submit_shares_success(slice(payload, len)) else {
            return -1;
        };
        (*channel_id, *new_submits_accepted_count) = p;
        0
    }

    /// The error code is copied NUL-terminated and truncated to
    /// `error_code_size - 1` bytes (nothing is written for size 0).
    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_submit_shares_error(
        payload: *const u8,
        len: u32,
        channel_id: *mut u32,
        seq_num: *mut u32,
        error_code: *mut c_char,
        error_code_size: usize,
    ) -> c_int {
        let Some((id, seq, code)) = submit_shares_error(slice(payload, len)) else {
            return -1;
        };
        (*channel_id, *seq_num) = (id, seq);
        if error_code_size > 0 {
            let n = code.len().min(error_code_size - 1);
            core::ptr::copy_nonoverlapping(code.as_ptr(), error_code as *mut u8, n);
            *error_code.add(n) = 0;
        }
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_parse_new_extended_mining_job(
        payload: *const u8,
        len: u32,
        channel_id_out: *mut u32,
    ) -> *mut ExtJob {
        let parsed = new_extended_mining_job(slice(payload, len));
        let channel_id = match &parsed {
            Ok(j) => Some(j.channel_id),
            Err(id) => *id,
        };
        if let (Some(id), false) = (channel_id, channel_id_out.is_null()) {
            *channel_id_out = id;
        }
        let Ok(j) = parsed else {
            return core::ptr::null_mut();
        };

        let ntime = j.min_ntime.unwrap_or(0);
        // A job with min_ntime is for the current prevhash (the C code's
        // convention: clean_jobs = has min_ntime).
        let job = alloc_ext_job(
            j.job_id,
            j.version,
            [0; 32],
            ntime,
            0,
            j.min_ntime.is_some(),
            &j.merkle_path,
            j.coinbase_prefix,
            j.coinbase_suffix,
            &[],
        );
        if !job.is_null() {
            (*job).version_rolling_allowed = j.version_rolling_allowed;
        }
        job
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_ext_job_free(job: *mut ExtJob) {
        if job.is_null() {
            return;
        }
        free((*job).coinbase_prefix as _);
        free((*job).coinbase_suffix as _);
        free(job as _);
    }

    #[no_mangle]
    pub unsafe extern "C" fn sv2_target_to_pdiff(target: *const [u8; 32]) -> u32 {
        target_to_pdiff(&*target)
    }
}
