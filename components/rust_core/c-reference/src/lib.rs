//! Host builds of the former C/C++ implementations (see build.rs), plus
//! the platform functions they need.

use core::ffi::{c_char, c_int};
use rust_core::coinbase::{CoinbaseResult, EspErr};

extern "C" {
    pub fn c_coinbase_process(
        coinbase_1: *const c_char,
        coinbase_2: *const c_char,
        block_version: u32,
        nbits: u32,
        extranonce1: *const c_char,
        extranonce2_len: c_int,
        user_address: *const c_char,
        result: *mut CoinbaseResult,
    ) -> EspErr;
}

/// `mbedtls_sha256()` for the host builds of the C reference code.
///
/// # Safety
/// `input` must point to `len` bytes and `output` to 32 writable bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256(
    input: *const u8,
    len: usize,
    output: *mut u8,
    is224: c_int,
) -> c_int {
    use sha2::Digest;
    assert_eq!(is224, 0, "SHA-224 is not used");
    let data = if len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(input, len)
    };
    let hash = sha2::Sha256::digest(data);
    std::ptr::copy_nonoverlapping(hash.as_ptr(), output, 32);
    0
}

// ---- mbedTLS stand-ins for the C reference code (shim/mbedtls/*.h) ----
// The C side allocates the contexts; they are opaque storage for the Rust
// state, large enough as checked below.

use chacha20poly1305::aead::{AeadInPlace, KeyInit};
use hmac::Mac;

type Sha256State = Option<sha2::Sha256>;
type HmacState = Option<hmac::Hmac<sha2::Sha256>>;
#[allow(dead_code)] // only used in the assertions below
const CTX_STORAGE: usize = 64 * 8;
const _: () = assert!(
    std::mem::size_of::<Sha256State>() <= CTX_STORAGE && std::mem::align_of::<Sha256State>() <= 8
);
const _: () = assert!(
    std::mem::size_of::<HmacState>() <= CTX_STORAGE && std::mem::align_of::<HmacState>() <= 8
);

/// # Safety
/// `ctx` points to an `mbedtls_sha256_context`.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256_init(ctx: *mut Sha256State) {
    ctx.write(None);
}
/// # Safety
/// `ctx` was initialized with `mbedtls_sha256_init`.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256_starts(ctx: *mut Sha256State, is224: c_int) -> c_int {
    assert_eq!(is224, 0);
    *ctx = Some(<sha2::Sha256 as sha2::Digest>::new());
    0
}
/// # Safety
/// `ctx` was started; `input` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256_update(
    ctx: *mut Sha256State,
    input: *const u8,
    len: usize,
) -> c_int {
    let data = if len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(input, len)
    };
    sha2::Digest::update((*ctx).as_mut().expect("started"), data);
    0
}
/// # Safety
/// `ctx` was started; `out` holds 32 bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256_finish(ctx: *mut Sha256State, out: *mut u8) -> c_int {
    let hash = sha2::Digest::finalize((*ctx).take().expect("started"));
    std::ptr::copy_nonoverlapping(hash.as_ptr(), out, 32);
    0
}
/// # Safety
/// `ctx` was initialized.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_sha256_free(ctx: *mut Sha256State) {
    *ctx = None;
}

/// The "info" is the name of the hash (SHA1 = 4, SHA256 = 9 as in mbedTLS).
#[no_mangle]
pub extern "C" fn mbedtls_md_info_from_type(md_type: c_int) -> *const u8 {
    match md_type {
        4 => c"sha1".as_ptr() as _,
        _ => c"sha256".as_ptr() as _,
    }
}

/// One-shot HMAC (SHA-1 or SHA-256, by the info from
/// `mbedtls_md_info_from_type`).
///
/// # Safety
/// Valid buffers; `output` holds the digest size.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_hmac(
    info: *const u8,
    key: *const u8,
    keylen: usize,
    input: *const u8,
    ilen: usize,
    output: *mut u8,
) -> c_int {
    let s = |p: *const u8, n: usize| {
        if n == 0 {
            &[][..]
        } else {
            std::slice::from_raw_parts(p, n)
        }
    };
    let (key, input) = (s(key, keylen), s(input, ilen));
    let sha1 = std::ffi::CStr::from_ptr(info as _).to_bytes() == b"sha1";
    let mac = if sha1 {
        let mut m = <hmac::Hmac<sha1::Sha1> as Mac>::new_from_slice(key).unwrap();
        m.update(input);
        m.finalize().into_bytes().to_vec()
    } else {
        let mut m = <hmac::Hmac<sha2::Sha256> as Mac>::new_from_slice(key).unwrap();
        m.update(input);
        m.finalize().into_bytes().to_vec()
    };
    std::ptr::copy_nonoverlapping(mac.as_ptr(), output, mac.len());
    0
}
/// # Safety
/// `ctx` points to an `mbedtls_md_context_t`.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_init(ctx: *mut HmacState) {
    ctx.write(None);
}
#[no_mangle]
pub extern "C" fn mbedtls_md_setup(_ctx: *mut HmacState, _info: *const u8, hmac: c_int) -> c_int {
    assert_eq!(hmac, 1);
    0
}
/// # Safety
/// `ctx` was initialized; `key` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_hmac_starts(
    ctx: *mut HmacState,
    key: *const u8,
    len: usize,
) -> c_int {
    let key = if len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(key, len)
    };
    *ctx = Some(<hmac::Hmac<sha2::Sha256> as Mac>::new_from_slice(key).unwrap());
    0
}
/// # Safety
/// `ctx` was started; `input` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_hmac_update(
    ctx: *mut HmacState,
    input: *const u8,
    len: usize,
) -> c_int {
    let data = if len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(input, len)
    };
    (*ctx).as_mut().expect("started").update(data);
    0
}
/// # Safety
/// `ctx` was started; `out` holds 32 bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_hmac_finish(ctx: *mut HmacState, out: *mut u8) -> c_int {
    let tag = (*ctx).take().expect("started").finalize().into_bytes();
    std::ptr::copy_nonoverlapping(tag.as_ptr(), out, 32);
    0
}
/// # Safety
/// `ctx` was initialized.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_md_free(ctx: *mut HmacState) {
    *ctx = None;
}

#[no_mangle]
pub extern "C" fn mbedtls_chachapoly_init(_ctx: *mut [u8; 32]) {}
#[no_mangle]
pub extern "C" fn mbedtls_chachapoly_free(_ctx: *mut [u8; 32]) {}
/// # Safety
/// `ctx` points to an `mbedtls_chachapoly_context`, `key` to 32 bytes.
#[no_mangle]
pub unsafe extern "C" fn mbedtls_chachapoly_setkey(
    ctx: *mut [u8; 32],
    key: *const [u8; 32],
) -> c_int {
    *ctx = *key;
    0
}

unsafe fn chacha_args<'a>(
    len: usize,
    aad: *const u8,
    aad_len: usize,
    input: *const u8,
) -> (&'a [u8], &'a [u8]) {
    let aad = if aad.is_null() || aad_len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(aad, aad_len)
    };
    let input = if len == 0 {
        &[][..]
    } else {
        std::slice::from_raw_parts(input, len)
    };
    (aad, input)
}

/// # Safety
/// Pointers as in mbedTLS.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn mbedtls_chachapoly_encrypt_and_tag(
    ctx: *mut [u8; 32],
    len: usize,
    nonce: *const [u8; 12],
    aad: *const u8,
    aad_len: usize,
    input: *const u8,
    output: *mut u8,
    tag: *mut u8,
) -> c_int {
    let (aad, input) = chacha_args(len, aad, aad_len, input);
    let mut buf = input.to_vec();
    let t = chacha20poly1305::ChaCha20Poly1305::new((&*ctx).into())
        .encrypt_in_place_detached((&*nonce).into(), aad, &mut buf)
        .unwrap();
    std::ptr::copy_nonoverlapping(buf.as_ptr(), output, len);
    std::ptr::copy_nonoverlapping(t.as_ptr(), tag, 16);
    0
}

/// # Safety
/// Pointers as in mbedTLS.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn mbedtls_chachapoly_auth_decrypt(
    ctx: *mut [u8; 32],
    len: usize,
    nonce: *const [u8; 12],
    aad: *const u8,
    aad_len: usize,
    tag: *const [u8; 16],
    input: *const u8,
    output: *mut u8,
) -> c_int {
    let (aad, input) = chacha_args(len, aad, aad_len, input);
    let mut buf = input.to_vec();
    let ok = chacha20poly1305::ChaCha20Poly1305::new((&*ctx).into())
        .decrypt_in_place_detached((&*nonce).into(), aad, &mut buf, (&*tag).into())
        .is_ok();
    if !ok {
        return -0x0056; // MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED
    }
    std::ptr::copy_nonoverlapping(buf.as_ptr(), output, len);
    0
}

/// The current settings path (`settings_apply.rs` on the Rust parsers),
/// logging into the reference's config log (cpp-shim/cfg_stub.cpp) in its
/// format; the getters return their default like the stub.
mod settings_current {
    use core::ffi::{c_char, CStr};
    use rust_core::http_settings::{self, Write};
    use rust_core::settings_apply::{apply, Applied, Store};

    extern "C" {
        fn cfg_log_append(line: *const c_char);
    }

    struct Log;

    fn log(line: String) {
        let c = std::ffi::CString::new(line).unwrap();
        unsafe { cfg_log_append(c.as_ptr()) };
    }

    impl Store for Log {
        fn set_u16(&mut self, key: &[u8], v: u16) {
            log(format!("{}=u16:{v}\n", String::from_utf8_lossy(key)));
        }
        fn set_u64(&mut self, key: &[u8], v: u64) {
            log(format!("{}=u64:{v}\n", String::from_utf8_lossy(key)));
        }
        fn set_str(&mut self, key: &[u8], v: Option<&[u8]>) {
            let v = String::from_utf8_lossy(v.unwrap_or_default());
            log(format!("{}=s:{v}\n", String::from_utf8_lossy(key)));
        }
        fn get_u16(&mut self, _: &[u8], def: u16) -> u16 {
            def
        }
    }

    /// As the handlers: invalid JSON applies nothing. The settings build
    /// is a NerdAxe (fanPidUseMax applies).
    unsafe fn run(
        body: *const c_char,
        parse: impl Fn(&rust_core::ajson::Json) -> Vec<Write>,
    ) -> (bool, Applied) {
        let mut a = Applied::default();
        let body = CStr::from_ptr(body).to_bytes();
        let Ok(doc) = rust_core::ajson::deserialize(body) else {
            return (false, a);
        };
        for w in parse(&doc) {
            apply(&w, &mut a, true, &mut Log);
        }
        (true, a)
    }

    #[no_mangle]
    pub unsafe extern "C" fn new_settings_v1(body: *const c_char, dual_pool: bool) -> bool {
        run(body, |d| http_settings::settings_v1(d, dual_pool)).0
    }

    #[no_mangle]
    pub unsafe extern "C" fn new_settings_v2(
        body: *const c_char,
        verify_changed: *mut bool,
    ) -> bool {
        let (ok, a) = run(body, http_settings::settings_v2);
        *verify_changed = a.verify_changed[0];
        *verify_changed.add(1) = a.verify_changed[1];
        ok
    }

    #[no_mangle]
    pub unsafe extern "C" fn new_influx(body: *const c_char) -> bool {
        run(body, http_settings::settings_influx).0
    }

    #[no_mangle]
    pub unsafe extern "C" fn new_alert(body: *const c_char) -> bool {
        run(body, http_settings::settings_alert).0
    }
}
