//! Logging through ESP-IDF's `esp_log_write()` on the device, in the format
//! of the `ESP_LOGx` macros (without colours). Silent on the host.

#[derive(Clone, Copy)]
pub enum Level {
    Error = 1,
    Warn = 2,
    Info = 3,
}

pub fn write(level: Level, tag: &core::ffi::CStr, msg: &str) {
    #[cfg(target_os = "espidf")]
    {
        use core::ffi::{c_char, c_int};
        extern "C" {
            fn esp_log_write(level: c_int, tag: *const c_char, format: *const c_char, ...);
            fn esp_log_timestamp() -> u32;
        }
        let letter = match level {
            Level::Error => b'E',
            Level::Warn => b'W',
            Level::Info => b'I',
        };
        // SAFETY: format string and arguments match; msg is passed with its length.
        unsafe {
            esp_log_write(
                level as c_int,
                tag.as_ptr(),
                c"%c (%lu) %s: %.*s\n".as_ptr(),
                letter as c_int,
                esp_log_timestamp() as core::ffi::c_ulong,
                tag.as_ptr(),
                msg.len() as c_int,
                msg.as_ptr() as *const c_char,
            );
        }
    }
    #[cfg(not(target_os = "espidf"))]
    let _ = (level, tag, msg);
}

macro_rules! log {
    ($level:ident, $tag:expr, $($arg:tt)*) => {
        $crate::log::write($crate::log::Level::$level, $tag, &format!($($arg)*))
    };
}
pub(crate) use log;
