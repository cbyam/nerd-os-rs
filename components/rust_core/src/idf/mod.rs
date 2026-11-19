//! ESP-IDF C APIs used directly from Rust: the bindings generated from the
//! IDF 5.3 headers (`raw`, firmware only) and thin wrappers over them. On the
//! host every call fails (stubs), so the crate still builds and tests there.

pub mod adc;
pub mod http_client;
pub mod httpd;
pub mod i2c;
pub mod nvs;
#[cfg(target_os = "espidf")]
pub mod raw;
pub mod task;
pub mod twai;

use core::ffi::c_int;

/// An IDF constant: from the bindings on the firmware (an expression over
/// the names in `raw`), the given value on the host (tests).
macro_rules! idf_const {
    ($(#[$m:meta])* $vis:vis $name:ident: $t:ty = $raw:expr, $host:expr) => {
        $(#[$m])*
        #[cfg(target_os = "espidf")]
        $vis const $name: $t = {
            #[allow(unused_imports)]
            use crate::idf::raw::*;
            ($raw) as $t
        };
        $(#[$m])*
        #[cfg(not(target_os = "espidf"))]
        $vis const $name: $t = $host;
    };
}
pub(crate) use idf_const;

/// `esp_err_t`
pub type EspErr = c_int;
#[cfg(target_os = "espidf")]
const _: () = assert!(core::mem::size_of::<raw::esp_err_t>() == core::mem::size_of::<EspErr>());
idf_const!(pub ESP_OK: EspErr = ESP_OK, 0);
idf_const!(pub ESP_FAIL: EspErr = ESP_FAIL, -1);
idf_const!(pub ESP_ERR_INVALID_ARG: EspErr = ESP_ERR_INVALID_ARG, 0x102);

/// `TickType_t`
#[cfg(target_os = "espidf")]
pub type Ticks = raw::TickType_t;
#[cfg(not(target_os = "espidf"))]
pub type Ticks = u32;

idf_const!(pub TICK_RATE_HZ: u32 = CONFIG_FREERTOS_HZ, 100);

/// `pdMS_TO_TICKS()`
pub const fn ms_to_ticks(ms: u32) -> Ticks {
    ((ms as u64 * TICK_RATE_HZ as u64) / 1000) as Ticks
}
