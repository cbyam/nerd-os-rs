//! The web UI files (port of `main/http_server/handler_file.cpp`): mounts
//! the SPIFFS partition (`init_fs()`; a failure means recovery mode) and
//! serves its gzipped files with the type and cache policy of
//! `http_file.rs`; a missing page redirects to the portal, a missing asset
//! is a 404.

use crate::idf::idf_const;
use core::ffi::{c_char, CStr};
use std::io::Read;

use crate::http_server::{ServerCtx, SCRATCH_SIZE};
use crate::idf::httpd::{self, err, Req};
use crate::idf::{EspErr, ESP_FAIL, ESP_OK};
use crate::log::log;

const TAG: &CStr = c"http_file";
idf_const!(
    /// `FILE_PATH_MAX`: `ESP_VFS_PATH_MAX + 128`
    FILE_PATH_MAX: usize = ESP_VFS_PATH_MAX + 128, 15 + 128
);
idf_const!(ESP_ERR_NOT_FOUND: EspErr = ESP_ERR_NOT_FOUND, 0x105);

#[cfg(target_os = "espidf")]
pub use crate::idf::raw::esp_vfs_spiffs_conf_t as SpiffsConf;
#[cfg(not(target_os = "espidf"))]
/// `esp_vfs_spiffs_conf_t` (host model)
#[repr(C)]
pub struct SpiffsConf {
    pub base_path: *const c_char,
    pub partition_label: *const c_char,
    pub max_files: usize,
    pub format_if_mount_failed: bool,
}

#[cfg(target_os = "espidf")]
mod sys {
    pub use crate::idf::raw::{esp_spiffs_info, esp_vfs_spiffs_register};
}

#[cfg(not(target_os = "espidf"))]
mod sys {
    use super::*;
    pub unsafe fn esp_vfs_spiffs_register(_: *const SpiffsConf) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn esp_spiffs_info(_: *const c_char, _: *mut usize, _: *mut usize) -> EspErr {
        ESP_FAIL
    }
}

/// Mounts the web UI partition at "".
pub fn init_fs() -> EspErr {
    let conf = SpiffsConf {
        base_path: c"".as_ptr(),
        partition_label: core::ptr::null(),
        max_files: 5,
        format_if_mount_failed: false,
    };
    let ret = unsafe { sys::esp_vfs_spiffs_register(&conf) };
    if ret != ESP_OK {
        match ret {
            ESP_FAIL => log!(Error, TAG, "Failed to mount or format filesystem"),
            ESP_ERR_NOT_FOUND => log!(Error, TAG, "Failed to find SPIFFS partition"),
            e => log!(
                Error,
                TAG,
                "Failed to initialize SPIFFS ({})",
                crate::idf::nvs::err_name(e)
            ),
        }
        return ESP_FAIL;
    }
    let (mut total, mut used) = (0usize, 0usize);
    let ret = unsafe { sys::esp_spiffs_info(core::ptr::null(), &mut total, &mut used) };
    if ret != ESP_OK {
        log!(
            Error,
            TAG,
            "Failed to get SPIFFS partition information ({})",
            crate::idf::nvs::err_name(ret)
        );
    } else {
        log!(Info, TAG, "Partition size: total: {total}, used: {used}");
    }
    ESP_OK
}

unsafe fn redirect_portal(req: *mut Req) -> EspErr {
    log!(Info, TAG, "Redirecting to portal");
    httpd::httpd_resp_set_status(req, c"302 Found".as_ptr());
    httpd::httpd_resp_set_type(req, c"text/plain".as_ptr());
    httpd::httpd_resp_set_hdr(req, c"Location".as_ptr(), c"/".as_ptr());
    httpd::httpd_resp_send(
        req,
        c"Redirect to the captive portal".as_ptr(),
        httpd::RESP_USE_STRLEN,
    );
    ESP_OK
}

fn open(path: &[u8]) -> std::io::Result<std::fs::File> {
    use std::os::unix::ffi::OsStrExt;
    std::fs::File::open(std::ffi::OsStr::from_bytes(path))
}

/// `rest_common_get_handler()`: the file of the request.
///
/// # Safety
/// `req` a request of a route with the server context.
pub unsafe extern "C" fn file_handler(req: *mut Req) -> EspErr {
    let ctx = (*req).user_ctx as *mut ServerCtx;
    let base = CStr::from_ptr((*ctx).base_path.as_ptr()).to_bytes();
    let uri = CStr::from_ptr((*req).uri.as_ptr());
    let Some(f) =
        crate::http_file::resolve(base, uri.to_bytes()).filter(|f| f.path.len() < FILE_PATH_MAX)
    else {
        return httpd::httpd_resp_send_err(
            req,
            err::E500_INTERNAL_SERVER_ERROR,
            c"File path too long".as_ptr(),
        );
    };
    let path = String::from_utf8_lossy(&f.path).into_owned();

    // do not MIME-sniff
    httpd::httpd_resp_set_hdr(req, c"X-Content-Type-Options".as_ptr(), c"nosniff".as_ptr());

    let mut file = match open(&f.path) {
        Ok(file) => file,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            if f.asset {
                // a missing asset: 404, no portal redirect
                let errno = e.raw_os_error().unwrap_or(0);
                log!(Error, TAG, "Failed to open file: {path}, errno: {errno}");
                httpd::httpd_resp_send_err(req, err::E404_NOT_FOUND, c"Not found".as_ptr());
                return ESP_OK;
            }
            log!(Error, TAG, "page not found: {}", uri.to_string_lossy());
            return redirect_portal(req);
        }
        Err(_) => {
            return httpd::httpd_resp_send_err(
                req,
                err::E500_INTERNAL_SERVER_ERROR,
                c"Failed to open file".as_ptr(),
            )
        }
    };

    log!(Info, TAG, "Sending {path}");

    // the file's headers (type, cache policy, gzip) only go with the file,
    // not with an error response
    let mut started = false;
    let start = |req: *mut Req| {
        httpd::httpd_resp_set_type(req, f.content_type.as_ptr());
        httpd::httpd_resp_set_hdr(req, c"Cache-Control".as_ptr(), f.cache_control.as_ptr());
        httpd::httpd_resp_set_hdr(req, c"Content-Encoding".as_ptr(), c"gzip".as_ptr());
    };
    let chunk =
        core::slice::from_raw_parts_mut((*ctx).scratch.as_mut_ptr() as *mut u8, SCRATCH_SIZE);
    loop {
        let n = match file.read(chunk) {
            Ok(0) => break,
            Ok(n) => n,
            Err(_) => {
                log!(Error, TAG, "Read error while sending {path}");
                if !started {
                    return httpd::httpd_resp_send_err(
                        req,
                        err::E500_INTERNAL_SERVER_ERROR,
                        c"Read error".as_ptr(),
                    );
                }
                // headers and part of the body are out: no final chunk, so
                // the client sees a truncated transfer
                return ESP_FAIL;
            }
        };
        if !started {
            start(req);
            started = true;
        }
        if httpd::httpd_resp_send_chunk(req, chunk.as_ptr() as *const c_char, n as isize) != ESP_OK
        {
            log!(Error, TAG, "Failed to send file chunk");
            return ESP_FAIL;
        }
    }
    if !started {
        // an empty file
        start(req);
    }
    log!(Info, TAG, "File sending complete");
    httpd::httpd_resp_send_chunk(req, core::ptr::null(), 0);
    ESP_OK
}
