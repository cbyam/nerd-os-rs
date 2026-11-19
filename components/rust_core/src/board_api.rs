//! The board for the Rust modules that reached it through C (port of
//! `main/boards/board_api.cpp`, the bridge to the C++ `Board`, which by now
//! only forwarded to `board.rs`): the same `bapi_*` names, so the modules'
//! firmware `sys` blocks re-export these instead of declaring externs (the
//! host keeps its stubs).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int};

use crate::board::ffi as b;
use crate::board_core::CHIP_TEMPS;

fn info() -> b::Info {
    let mut i = core::mem::MaybeUninit::uninit();
    unsafe {
        b::board_info(i.as_mut_ptr());
        i.assume_init()
    }
}

pub unsafe fn bapi_is_initialized() -> bool {
    b::board_is_initialized()
}
pub unsafe fn bapi_is_buck_initialized() -> bool {
    info().buck_initialized
}
pub unsafe fn bapi_asic_voltage_millis() -> u16 {
    info().asic_voltage_mv as u16
}
pub unsafe fn bapi_set_voltage(volts: f32) {
    b::board_set_voltage(volts);
}
pub unsafe fn bapi_asic_frequency() -> u16 {
    info().asic_frequency as u16
}
pub unsafe fn bapi_set_asic_frequency(mhz: f32) -> bool {
    b::board_set_asic_frequency(mhz)
}
pub unsafe fn bapi_vr_frequency() -> u32 {
    info().vr_frequency
}
pub unsafe fn bapi_set_vr_frequency(hz: u32) {
    b::board_set_vr_frequency(hz)
}
pub unsafe fn bapi_request_chip_temps() {
    b::board_request_chip_temps()
}
pub unsafe fn bapi_max_chip_temp() -> f32 {
    CHIP_TEMPS.max()
}
pub unsafe fn bapi_asic_count() -> c_int {
    info().asic_count
}
pub unsafe fn bapi_chip_temp(i: c_int) -> f32 {
    CHIP_TEMPS.get(i)
}
pub unsafe fn bapi_request_buck_telemetry() {
    b::board_request_buck_telemetry()
}

/// 0 Vin, 1 Iin, 2 Pin, 3 Pout, 4 Vout, 5 Iout, 6 VR temperature, 7 VR
/// temperature (internal); `board_power_value()` orders them differently.
pub unsafe fn bapi_power(what: c_int) -> f32 {
    match what {
        0 => b::board_power_value(0),
        1 => b::board_power_value(1),
        2 => b::board_power_value(2),
        3 => b::board_power_value(5),
        4 => b::board_power_value(3),
        5 => b::board_power_value(4),
        6 => b::board_vr_temp(),
        _ => b::board_vr_temp_internal(),
    }
}

pub unsafe fn bapi_fault(status: *mut u32) -> c_int {
    b::board_fault(status)
}
pub unsafe fn bapi_num_temp_sensors() -> c_int {
    info().num_temp_sensors
}
pub unsafe fn bapi_temperature(i: c_int) -> f32 {
    b::board_temperature(i)
}
pub unsafe fn bapi_shutdown() {
    b::board_shutdown()
}
pub unsafe fn bapi_invert_fan_polarity() -> bool {
    info().fan_invert
}
pub unsafe fn bapi_set_fan_polarity(invert: bool) {
    b::board_set_fan_polarity(invert)
}
pub unsafe fn bapi_num_fans() -> c_int {
    info().num_fans
}
pub unsafe fn bapi_set_fan_speed(ch: c_int, fraction: f32) {
    b::board_set_fan_speed_ch(ch, fraction)
}
pub unsafe fn bapi_fan_rpm(ch: c_int) -> u16 {
    b::board_fan_rpm(ch)
}

/// target temperature, P, I, D of channel `ch` into `out[0..4]`
pub unsafe fn bapi_pid_settings(ch: c_int, out: *mut u16) {
    let p = info().pid[(ch & 1) as usize];
    let out = core::slice::from_raw_parts_mut(out, 4);
    out.copy_from_slice(&[p.target_temp, p.p, p.i, p.d]);
}

pub unsafe fn bapi_fan_mode(ch: c_int) -> c_int {
    info().fan_mode[(ch & 1) as usize]
}
pub unsafe fn bapi_set_board_error(error: c_int, status: u32) {
    crate::system::set_error(error, status)
}
pub unsafe fn bapi_set_board_error_if_none(error: c_int, status: u32) {
    crate::system::set_error_if_none(error, status)
}
pub unsafe fn bapi_clear_board_error(error: c_int) {
    crate::system::clear_error(error)
}
pub unsafe fn bapi_is_can_slave() -> bool {
    b::board_is_can_slave()
}
pub unsafe fn bapi_has_hashrate_counter() -> bool {
    info().has_hash_counter
}
pub unsafe fn bapi_mining_agent() -> *const c_char {
    info().mining_agent
}
pub unsafe fn bapi_asic_model() -> *const c_char {
    info().asic_model
}
pub unsafe fn bapi_device_model() -> *const c_char {
    info().device_model
}
pub unsafe fn bapi_asic_min_difficulty(dual_pool: bool) -> u32 {
    let i = info();
    if dual_pool {
        i.min_difficulty_dual_pool
    } else {
        i.min_difficulty
    }
}
pub unsafe fn bapi_asic_max_difficulty() -> u32 {
    info().max_difficulty
}
