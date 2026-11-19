//! Differential tests of the EMC2101 fan controller/temperature sensor:
//! rust_core::emc2101 against the former C++ driver
//! (c-drivers/nerdaxe/EMC2101.cpp, verbatim) on the recording I2C stub: the
//! same register accesses and results, except for duty cycles above 1 and
//! tach readings below 83 (reported as 0 RPM).

use std::ffi::{c_char, CStr};
use std::sync::Mutex;

use c_reference as _;
use rust_core::emc2101;
use rust_core::i2c::RegBus;

extern "C" {
    fn i2c_stub_reset();
    fn i2c_stub_set(reg: u8, value: u16);
    fn i2c_stub_fail(fail: bool);
    fn i2c_stub_trace() -> *const c_char;
    fn i2c_stub_clear_trace();
    fn ref2101_init(invert: bool) -> bool;
    fn ref2101_polarity(invert: bool) -> bool;
    fn ref2101_set_speed(percent: f32);
    fn ref2101_get_speed() -> u16;
    fn ref2101_external_temp() -> f32;
    fn ref2101_internal_temp() -> u8;
    fn ref2101_ideality(v: u8);
    fn ref2101_beta(v: u8);
}

static LOCK: Mutex<()> = Mutex::new(());

struct Fake {
    regs: [u8; 256],
    trace: String,
    fail: bool,
}

impl Fake {
    fn new(fail: bool) -> Self {
        Self {
            regs: [0; 256],
            trace: String::new(),
            fail,
        }
    }
}

impl RegBus for Fake {
    fn read(&mut self, addr: u8, reg: u8) -> Option<u8> {
        let mut v = [0];
        self.read_into(addr, reg, &mut v).then_some(v[0])
    }
    fn write(&mut self, addr: u8, reg: u8, value: u8) -> bool {
        self.trace += &format!("wr {addr:02x} {reg:02x} {value:02x}\n");
        !self.fail
    }
    fn read_into(&mut self, addr: u8, reg: u8, out: &mut [u8]) -> bool {
        self.trace += &format!("rr {addr:02x} {reg:02x} {}\n", out.len());
        if self.fail {
            return false;
        }
        for (i, b) in out.iter_mut().enumerate() {
            *b = self.regs[reg.wrapping_add(i as u8) as usize];
        }
        true
    }
}

unsafe fn ref_trace() -> String {
    let t = CStr::from_ptr(i2c_stub_trace())
        .to_string_lossy()
        .into_owned();
    i2c_stub_clear_trace();
    t
}

unsafe fn set(f: &mut Fake, reg: u8, v: u8) {
    i2c_stub_set(reg, v as u16);
    f.regs[reg as usize] = v;
}

#[test]
fn init_polarity_settings() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    unsafe {
        for fail in [false, true] {
            for invert in [false, true] {
                i2c_stub_reset();
                i2c_stub_fail(fail);
                let mut f = Fake::new(fail);
                assert_eq!(emc2101::init(&mut f, invert), ref2101_init(invert));
                assert_eq!(
                    emc2101::set_polarity(&mut f, invert),
                    ref2101_polarity(invert)
                );
                emc2101::set_ideality_factor(&mut f, 0x24);
                ref2101_ideality(0x24);
                emc2101::set_beta_compensation(&mut f, 0x00);
                ref2101_beta(0x00);
                let t = ref_trace();
                assert_eq!(f.trace, t);
                assert!(t.starts_with("wr 4c 03 04\n"), "{t}");
            }
        }
        i2c_stub_fail(false);
    }
}

#[test]
fn fan_speeds() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    unsafe {
        i2c_stub_reset();
        let mut f = Fake::new(false);
        let mut fractions: Vec<f32> = (-100..=1000).map(|i| i as f32 / 1000.0).collect();
        fractions.extend([0.999_999, 1.0, 0.015_873, 0.5, 1e-9, -0.0]);
        for p in fractions {
            f.trace.clear();
            emc2101::set_speed(&mut f, p);
            ref2101_set_speed(p);
            assert_eq!(f.trace, ref_trace(), "{p}");
        }
        // the boards' setFanSpeed(100) is full speed
        f.trace.clear();
        emc2101::set_speed(&mut f, 100.0);
        assert_eq!(f.trace, "wr 4c 4c 3f\n");
    }
}

#[test]
fn tach_and_temperatures() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    unsafe {
        i2c_stub_reset();
        let mut f = Fake::new(false);
        let mut checked = 0;
        for reading in 0..=0xffffu32 {
            let [lsb, msb] = (reading as u16).to_le_bytes();
            set(&mut f, 0x46, lsb);
            set(&mut f, 0x47, msb);
            let rust = emc2101::speed(&mut f);
            // readings below 83 are out of the RPM range: 0
            if reading < 83 {
                assert_eq!(rust, 0);
                continue;
            }
            assert_eq!(rust, ref2101_get_speed(), "{reading}");
            checked += 1;
        }
        assert_eq!(checked, 0x10000 - 83);
        f.trace.clear();
        i2c_stub_clear_trace();
        for msb in 0..=255u8 {
            for lsb in (0..=255u8).step_by(0x20) {
                set(&mut f, 0x01, msb);
                set(&mut f, 0x10, lsb);
                set(&mut f, 0x00, msb);
                let t = emc2101::external_temp(&mut f);
                assert_eq!(t.to_bits(), ref2101_external_temp().to_bits());
                assert_eq!(emc2101::internal_temp(&mut f), ref2101_internal_temp());
            }
        }
        assert_eq!(f.trace, ref_trace());
        assert!(f.trace.starts_with("rr 4c 01 1\nrr 4c 10 1\nrr 4c 00 1\n"));
        // bus errors: 0, after the first failed read
        i2c_stub_fail(true);
        f.fail = true;
        f.trace.clear();
        i2c_stub_clear_trace();
        assert_eq!(emc2101::speed(&mut f), ref2101_get_speed());
        assert_eq!(emc2101::external_temp(&mut f), ref2101_external_temp());
        assert_eq!(emc2101::internal_temp(&mut f), ref2101_internal_temp());
        let t = ref_trace();
        assert_eq!(f.trace, t);
        assert_eq!(t, "rr 4c 46 1\nrr 4c 01 1\nrr 4c 00 1\n");
        i2c_stub_fail(false);
    }
}
