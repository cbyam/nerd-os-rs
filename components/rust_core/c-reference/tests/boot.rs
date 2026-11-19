//! Differential tests of the boot sequence: rust_core::boot against the
//! former app_main() (c-boot/, verbatim) on recording shims: for all reset
//! reasons, self test, CAN slave/master, pool modes, pool user, CAN and
//! hashrate counter settings and heap levels the same steps in the same
//! order, up to the suspend of the idle loop. Intended difference: an
//! invalid pool mode boots in failover (the reference suspends).

use std::ffi::{c_char, c_int, CStr};
use std::sync::Mutex;

use c_reference as _;
use rust_core::boot::{self, Hooks, PoolMode};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct In {
    reset_reason: c_int,
    self_test: bool,
    best_diff: u64,
    can_slave: bool,
    pool_mode: c_int,
    has_user: bool,
    can_enabled: bool,
    can_ext: bool,
    hr_counter: bool,
    shutdown_after: c_int,
    heap: [usize; 8],
}

extern "C" {
    fn ref_boot(input: *const In, buf: *mut c_char, len: usize) -> usize;
}

// the former code is global state
static LOCK: Mutex<()> = Mutex::new(());

const TASK_WDT: c_int = 6;

struct Rec {
    i: In,
    idle: c_int,
    t: String,
}

impl Rec {
    fn tr(&mut self, s: &str) {
        self.t.push_str(s);
        self.t.push(';');
    }
}

impl Hooks for Rec {
    fn early_init(&mut self) -> bool {
        self.tr("early_init");
        self.i.reset_reason == TASK_WDT
    }
    fn board_create(&mut self) {
        self.tr("board_create");
    }
    fn eth_init(&mut self) {
        self.tr("eth_init");
    }
    fn board_load_settings(&mut self) {
        self.tr("board_load_settings");
    }
    fn board_init(&mut self) {
        self.tr("board_init");
    }
    fn board_detect_eth(&mut self) {
        self.tr("board_detect_eth");
    }
    fn system_set_board(&mut self) {
        self.tr("system_set_board");
    }
    fn self_test_enabled(&mut self) -> bool {
        self.i.self_test
    }
    fn best_diff(&mut self) -> u64 {
        self.i.best_diff
    }
    fn self_test(&mut self) {
        self.tr("self_test");
    }
    fn delay_ms(&mut self, ms: u32) {
        self.tr(&format!("delay:{ms}"));
        if ms == boot::IDLE_MS {
            self.idle += 1;
        }
    }
    fn is_can_slave(&mut self) -> bool {
        self.i.can_slave
    }
    fn log_can_slave(&mut self) {}
    fn system_init(&mut self) {
        self.tr("system_init");
    }
    fn start_system_tasks(&mut self) {
        self.tr("task:SYSTEM_task");
        self.tr("task:power mangement");
        self.tr("startup_done");
    }
    fn can_init(&mut self) {
        self.tr("can_init");
    }
    fn init_asics(&mut self) {
        self.tr("init_asics");
    }
    fn start_can_slave_tasks(&mut self) {
        self.tr("task:can slave");
        self.tr("task:can result");
        self.tr("task:can telem");
    }
    fn has_hashrate_counter(&mut self) -> bool {
        self.i.hr_counter
    }
    fn start_hashrate_monitor(&mut self) {
        self.tr("hashrate");
    }
    fn pool_mode(&mut self) -> c_int {
        self.i.pool_mode
    }
    fn log_invalid_pool_mode(&mut self, _v: c_int) {}
    fn create_stratum_manager(&mut self, mode: PoolMode) {
        let m = match mode {
            PoolMode::Dual => "dual",
            PoolMode::Failover => "failover",
        };
        self.tr(&format!("stratum_manager:{m}"));
    }
    fn stratum_load_settings(&mut self) {
        self.tr("stratum_load_settings");
    }
    fn setup_network(&mut self) {
        self.tr("setup_network");
    }
    fn has_pool_user(&mut self) -> bool {
        self.i.has_user
    }
    fn ap_off_once(&mut self) {
        self.tr("ap_off_once");
    }
    fn discord_start(&mut self) {
        self.tr("discord_start");
    }
    fn otp_init(&mut self) {
        self.tr("otp_init");
    }
    fn sntp_start(&mut self) {
        self.tr("sntp_start");
    }
    fn discord_watchdog_alert(&mut self) {
        self.tr("watchdog_alert");
    }
    fn start_mining_tasks(&mut self) {
        for t in [
            "stratum miner",
            "asic result",
            "influx",
            "apis ticker",
            "wifi monitor",
        ] {
            self.tr(&format!("task:{t}"));
        }
    }
    fn can_master_wanted(&mut self) -> bool {
        self.i.can_enabled && self.i.can_ext
    }
    fn start_can_master(&mut self) {
        self.tr("can_init");
        self.tr("task:can master");
    }
    fn start_ota_task(&mut self) {
        self.tr("task:ota updater");
    }
    fn start_stratum_task(&mut self) {
        self.tr("task:stratum manager");
    }
    fn is_shutdown(&mut self) -> bool {
        self.idle >= self.i.shutdown_after
    }
    fn suspend(&mut self) {
        self.tr("logw:suspended");
        self.tr("suspend");
    }
    fn free_internal_heap(&mut self) -> usize {
        self.i.heap[(self.idle % 8) as usize]
    }
    fn log_low_heap(&mut self, _bytes: usize) {
        self.tr("logw:*** WARNING *** Free internal heap: %d bytes");
    }
}

fn reference(i: &In) -> String {
    let mut b = vec![0 as c_char; 8192];
    unsafe { ref_boot(i, b.as_mut_ptr(), b.len()) };
    unsafe { CStr::from_ptr(b.as_ptr()) }
        .to_str()
        .unwrap()
        .to_owned()
}

fn current(i: &In) -> String {
    let mut r = Rec {
        i: *i,
        idle: 0,
        t: String::new(),
    };
    boot::start(&mut r);
    while boot::idle(&mut r) {}
    r.t
}

#[test]
fn boot_matches_the_former_app_main() {
    let _g = LOCK.lock().unwrap();
    let mut n = 0;
    for bits in 0..(1 << 9) {
        let b = |k: u32| bits & (1 << k) != 0;
        for pool_mode in [0, 1, 2, 7] {
            for reset_reason in [1, TASK_WDT] {
                let i = In {
                    reset_reason,
                    self_test: b(0),
                    best_diff: if b(1) { 0 } else { 12345 },
                    can_slave: b(2),
                    pool_mode,
                    has_user: b(3),
                    can_enabled: b(4),
                    can_ext: b(5),
                    hr_counter: b(6),
                    shutdown_after: 1 + (bits as c_int >> 7),
                    heap: [20000, 9999, 10000, 5000, 20000, 0, 10001, 9000],
                };
                let old = reference(&i);
                let new = current(&i);
                if pool_mode > 1 && !i.can_slave {
                    // intended: failover instead of a suspended boot
                    let stop = "stratum_manager:failover;";
                    let at = new.find(stop).unwrap() + stop.len();
                    assert_eq!(old, format!("{}suspend;", &new[..at - stop.len()]), "{i:?}");
                    assert_eq!(current(&In { pool_mode: 0, ..i }), new, "{i:?}");
                } else {
                    assert_eq!(new, old, "{i:?}");
                }
                n += 1;
            }
        }
    }
    assert_eq!(n, 4096);
}
