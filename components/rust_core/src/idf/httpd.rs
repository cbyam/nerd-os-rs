//! `esp_http_server`: the structs and constants from the generated bindings,
//! the functions the Rust HTTP code uses. On the host every call fails.

use super::idf_const;
#[cfg(not(target_os = "espidf"))]
use core::ffi::c_char;
use core::ffi::{c_int, c_void};

use super::EspErr;

/// `httpd_handle_t`
pub type Handle = *mut c_void;

/// `CONFIG_HTTPD_MAX_URI_LEN`
pub const MAX_URI_LEN: usize = crate::sdkconfig::HTTPD_MAX_URI_LEN as usize;

/// `http_method`, `httpd_err_code_t` (enums: `u32` in the bindings)
pub type Method = u32;
pub type ErrCode = u32;

/// The firmware's types are the generated ones.
#[cfg(target_os = "espidf")]
mod types {
    pub use crate::idf::raw::{httpd_config_t as Config, httpd_req_t as Req, httpd_uri_t as Uri};

    /// `HTTPD_DEFAULT_CONFIG()`
    pub fn default_config() -> Config {
        Config {
            task_priority: 5,
            stack_size: 4096,
            core_id: 0x7fff_ffff,
            task_caps: super::super::task::CAP_INTERNAL | super::super::task::CAP_8BIT,
            server_port: 80,
            ctrl_port: 32768,
            max_open_sockets: 7,
            max_uri_handlers: 8,
            max_resp_headers: 8,
            backlog_conn: 5,
            lru_purge_enable: false,
            recv_wait_timeout: 5,
            send_wait_timeout: 5,
            global_user_ctx: core::ptr::null_mut(),
            global_user_ctx_free_fn: None,
            global_transport_ctx: core::ptr::null_mut(),
            global_transport_ctx_free_fn: None,
            enable_so_linger: false,
            linger_timeout: 0,
            keep_alive_enable: false,
            keep_alive_idle: 0,
            keep_alive_interval: 0,
            keep_alive_count: 0,
            open_fn: None,
            close_fn: None,
            uri_match_fn: None,
        }
    }
}

/// Host models with the fields the code uses.
#[cfg(not(target_os = "espidf"))]
mod types {
    use super::*;

    /// `httpd_req_t`
    #[repr(C)]
    pub struct Req {
        pub handle: Handle,
        pub method: c_int,
        pub uri: [c_char; MAX_URI_LEN + 1],
        pub content_len: usize,
        pub aux: *mut c_void,
        pub user_ctx: *mut c_void,
        pub sess_ctx: *mut c_void,
        pub free_ctx: Option<unsafe extern "C" fn(*mut c_void)>,
        pub ignore_sess_ctx_changes: bool,
    }

    /// `httpd_uri_t`
    pub struct Uri {
        pub uri: *const c_char,
        pub method: Method,
        pub handler: Option<HandlerFn>,
        pub user_ctx: *mut c_void,
        pub is_websocket: bool,
        pub handle_ws_control_frames: bool,
        pub supported_subprotocol: *const c_char,
    }

    /// `httpd_config_t` (the fields the server sets)
    pub struct Config {
        pub stack_size: usize,
        pub max_open_sockets: u16,
        pub max_uri_handlers: u16,
        pub lru_purge_enable: bool,
        pub keep_alive_enable: bool,
        pub recv_wait_timeout: u16,
        pub send_wait_timeout: u16,
        pub uri_match_fn: Option<unsafe extern "C" fn(*const c_char, *const c_char, usize) -> bool>,
        pub open_fn: Option<unsafe extern "C" fn(Handle, c_int) -> EspErr>,
        pub close_fn: Option<unsafe extern "C" fn(Handle, c_int)>,
        pub global_user_ctx: *mut c_void,
    }

    pub fn default_config() -> Config {
        Config {
            stack_size: 4096,
            max_open_sockets: 7,
            max_uri_handlers: 8,
            lru_purge_enable: false,
            keep_alive_enable: false,
            recv_wait_timeout: 5,
            send_wait_timeout: 5,
            uri_match_fn: None,
            open_fn: None,
            close_fn: None,
            global_user_ctx: core::ptr::null_mut(),
        }
    }
}

pub use types::*;

pub type HandlerFn = unsafe extern "C" fn(*mut Req) -> EspErr;

/// `http_method` (http_parser.h)
pub mod method {
    use super::Method;
    crate::idf::idf_const!(pub DELETE: Method = http_method_HTTP_DELETE, 0);
    crate::idf::idf_const!(pub GET: Method = http_method_HTTP_GET, 1);
    crate::idf::idf_const!(pub POST: Method = http_method_HTTP_POST, 3);
    crate::idf::idf_const!(pub OPTIONS: Method = http_method_HTTP_OPTIONS, 6);
    crate::idf::idf_const!(pub PATCH: Method = http_method_HTTP_PATCH, 28);
}

/// `httpd_err_code_t`
pub mod err {
    use super::ErrCode;
    crate::idf::idf_const!(pub E500_INTERNAL_SERVER_ERROR: ErrCode = httpd_err_code_t_HTTPD_500_INTERNAL_SERVER_ERROR, 0);
    crate::idf::idf_const!(pub E400_BAD_REQUEST: ErrCode = httpd_err_code_t_HTTPD_400_BAD_REQUEST, 3);
    crate::idf::idf_const!(pub E401_UNAUTHORIZED: ErrCode = httpd_err_code_t_HTTPD_401_UNAUTHORIZED, 4);
    crate::idf::idf_const!(pub E403_FORBIDDEN: ErrCode = httpd_err_code_t_HTTPD_403_FORBIDDEN, 5);
    crate::idf::idf_const!(pub E404_NOT_FOUND: ErrCode = httpd_err_code_t_HTTPD_404_NOT_FOUND, 6);
    crate::idf::idf_const!(pub E405_METHOD_NOT_ALLOWED: ErrCode = httpd_err_code_t_HTTPD_405_METHOD_NOT_ALLOWED, 7);
}

idf_const!(pub RESP_USE_STRLEN: isize = HTTPD_RESP_USE_STRLEN, -1);

/// lwIP `struct sockaddr_in6`
#[repr(C)]
#[derive(Default)]
pub struct SockaddrIn6 {
    pub sin6_len: u8,
    pub sin6_family: u8,
    pub sin6_port: u16,
    pub sin6_flowinfo: u32,
    pub sin6_addr: [u32; 4],
    pub sin6_scope_id: u32,
}

#[cfg(target_os = "espidf")]
const _: () = {
    use crate::idf::raw::sockaddr_in6;
    use core::mem::{offset_of, size_of};
    assert!(size_of::<SockaddrIn6>() == size_of::<sockaddr_in6>());
    assert!(offset_of!(SockaddrIn6, sin6_addr) == offset_of!(sockaddr_in6, sin6_addr));
};

#[cfg(target_os = "espidf")]
mod sys {
    use super::*;
    pub use crate::idf::raw::{
        httpd_register_err_handler, httpd_register_uri_handler, httpd_req_get_hdr_value_len,
        httpd_req_get_hdr_value_str, httpd_req_recv, httpd_req_to_sockfd, httpd_resp_send,
        httpd_resp_send_chunk, httpd_resp_send_err, httpd_resp_set_hdr, httpd_resp_set_status,
        httpd_resp_set_type, httpd_sess_trigger_close, httpd_start, httpd_uri_match_wildcard,
        lwip_close,
    };

    /// `struct sockaddr_in6` as the `struct sockaddr` it is passed as
    ///
    /// # Safety
    /// `addr`/`len` writable, `len` the size of `addr`.
    pub unsafe fn lwip_getpeername(s: c_int, addr: *mut SockaddrIn6, len: *mut u32) -> c_int {
        crate::idf::raw::lwip_getpeername(s, addr.cast(), len)
    }
}

#[cfg(not(target_os = "espidf"))]
#[allow(clippy::missing_safety_doc)]
mod sys {
    use super::*;
    use crate::idf::ESP_FAIL;
    pub unsafe fn httpd_start(_: *mut Handle, _: *const Config) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_register_uri_handler(_: Handle, _: *const Uri) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_register_err_handler(
        _: Handle,
        _: ErrCode,
        _: Option<unsafe extern "C" fn(*mut Req, ErrCode) -> EspErr>,
    ) -> EspErr {
        ESP_FAIL
    }
    pub unsafe extern "C" fn httpd_uri_match_wildcard(
        _: *const c_char,
        _: *const c_char,
        _: usize,
    ) -> bool {
        false
    }
    pub unsafe fn httpd_resp_send(_: *mut Req, _: *const c_char, _: isize) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_resp_send_chunk(_: *mut Req, _: *const c_char, _: isize) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_resp_set_type(_: *mut Req, _: *const c_char) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_resp_set_hdr(_: *mut Req, _: *const c_char, _: *const c_char) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_resp_set_status(_: *mut Req, _: *const c_char) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_resp_send_err(_: *mut Req, _: ErrCode, _: *const c_char) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_req_recv(_: *mut Req, _: *mut c_char, _: usize) -> c_int {
        -1
    }
    pub unsafe fn httpd_req_get_hdr_value_len(_: *mut Req, _: *const c_char) -> usize {
        0
    }
    pub unsafe fn httpd_req_get_hdr_value_str(
        _: *mut Req,
        _: *const c_char,
        _: *mut c_char,
        _: usize,
    ) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn httpd_req_to_sockfd(_: *mut Req) -> c_int {
        -1
    }
    pub unsafe fn httpd_sess_trigger_close(_: Handle, _: c_int) -> EspErr {
        ESP_FAIL
    }
    pub unsafe fn lwip_getpeername(_: c_int, _: *mut SockaddrIn6, _: *mut u32) -> c_int {
        -1
    }
    pub unsafe fn lwip_close(_: c_int) -> c_int {
        -1
    }
}

pub use sys::*;
