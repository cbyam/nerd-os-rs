//! Mining statistics of the pool manager (port of the counters in
//! `StratumManager`, `StratumManagerFallback` and `StratumManagerDualPool`):
//! accepted and rejected shares, pool and network difficulty, best session
//! and all-time difficulty, found blocks, the time of the last share
//! response. C API: `pool_stats.h`.
//!
//! In failover mode both pools share one set of counters (only one pool
//! mines at a time); in dual pool mode each pool has its own, and the
//! aggregate getters combine them as the C++ classes did.
//!
//! Every access goes through one mutex: the counters are updated from the
//! SV2 task, the ASIC result task and the CAN master, and 64-bit values and
//! the difficulty strings must not be read half-written on the 32-bit CPU.

use std::sync::Mutex;

use crate::coinbase::network_difficulty;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Failover,
    Dual,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Stats {
    pub accepted: [u64; 2],
    pub rejected: [u64; 2],
    pub best_session: [u64; 2],
    pub pool_difficulty: [u32; 2],
    pub network_difficulty: [f64; 2],
    /// all-time best difficulty (persisted by the caller)
    pub total_best: u64,
    /// found since the start (or the last session reset)
    pub found_blocks: u32,
    /// all-time (persisted by the caller)
    pub total_found_blocks: u32,
    /// `esp_timer_get_time()` of the last share response, 0: none yet
    pub last_response_us: u64,
    /// dual pool balance (percent for the primary)
    pub balance: u16,
}

/// What the caller has to do after a nonce was checked.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Event {
    /// persist `total_best` and send the best difficulty alert
    pub new_total_best: bool,
    pub total_best: u64,
    /// persist `total_found_blocks` and send the block found alert
    pub found_block: bool,
    pub total_found_blocks: u32,
    pub network_difficulty: f64,
}

pub struct PoolStats {
    mode: Mode,
    s: Stats,
}

impl PoolStats {
    pub fn new(mode: Mode, total_best: u64, total_found_blocks: u32) -> Self {
        PoolStats {
            mode,
            s: Stats {
                total_best,
                total_found_blocks,
                // the former m_balance until the settings are loaded
                balance: 50,
                ..Default::default()
            },
        }
    }

    pub fn stats(&self) -> Stats {
        self.s
    }

    /// The counter slot of a pool: failover shares slot 0; `None` for a
    /// pool that does not exist (dual pool mode checked the range).
    fn slot(&self, pool: i32) -> Option<usize> {
        match self.mode {
            Mode::Failover => Some(0),
            Mode::Dual => (0..2).contains(&pool).then_some(pool as usize),
        }
    }

    pub fn share_result(&mut self, pool: i32, accepted: bool, now_us: u64) {
        if accepted {
            self.shares_accepted(pool, 1, now_us);
        } else {
            if let Some(i) = self.slot(pool) {
                self.s.rejected[i] += 1;
            }
            self.s.last_response_us = now_us;
        }
    }

    /// `count` shares accepted at once (SV2 `SubmitShares.Success`; the
    /// count comes from the pool).
    pub fn shares_accepted(&mut self, pool: i32, count: u32, now_us: u64) {
        if let Some(i) = self.slot(pool) {
            self.s.accepted[i] = self.s.accepted[i].saturating_add(count as u64);
        }
        self.s.last_response_us = now_us;
    }

    pub fn set_pool_difficulty(&mut self, pool: i32, difficulty: u32) {
        if let Some(i) = self.slot(pool) {
            self.s.pool_difficulty[i] = difficulty;
        }
    }

    /// From the nbits of a job; 0 is ignored.
    pub fn set_network_difficulty(&mut self, pool: i32, nbits: u32) {
        if let (Some(i), true) = (self.slot(pool), nbits != 0) {
            self.s.network_difficulty[i] = network_difficulty(nbits);
        }
    }

    /// The persisted all-time values (on loading the settings).
    pub fn set_totals(&mut self, total_best: u64, total_found_blocks: u32) {
        self.s.total_best = total_best;
        self.s.total_found_blocks = total_found_blocks;
    }

    pub fn set_balance(&mut self, balance: u16) {
        self.s.balance = balance;
    }

    /// A nonce of `difficulty` for `pool`: session and all-time best.
    pub fn best_diff(&mut self, pool: i32, difficulty: f64, nbits: u32) -> Event {
        let Some(i) = self.slot(pool) else {
            return Event::default();
        };
        // the former (uint64_t) cast; `as` saturates (NaN gives 0)
        let d = difficulty as u64;
        if d > self.s.best_session[i] {
            self.s.best_session[i] = d;
        }
        if d <= self.s.total_best {
            return Event::default();
        }
        self.s.total_best = d;
        Event {
            new_total_best: true,
            total_best: d,
            network_difficulty: network_difficulty(nbits),
            ..Default::default()
        }
    }

    /// A nonce above the network difficulty is a block.
    pub fn found_block(&mut self, difficulty: f64, nbits: u32) -> Event {
        let network = network_difficulty(nbits);
        if difficulty <= network {
            return Event::default();
        }
        self.s.found_blocks += 1;
        self.s.total_found_blocks += 1;
        Event {
            found_block: true,
            total_found_blocks: self.s.total_found_blocks,
            network_difficulty: network,
            ..Default::default()
        }
    }

    /// Session counters of a pool (failover: the shared ones).
    pub fn reset_pool(&mut self, pool: i32) {
        if let Some(i) = self.slot(pool) {
            self.s.accepted[i] = 0;
            self.s.rejected[i] = 0;
            self.s.best_session[i] = 0;
        }
    }

    pub fn reset_session(&mut self) {
        self.reset_pool(0);
        self.reset_pool(1);
        self.s.found_blocks = 0;
    }

    pub fn reset_all_time(&mut self) {
        self.s.total_best = 0;
        self.s.total_found_blocks = 0;
    }

    // ---- aggregates, as the manager classes reported them ----

    pub fn accepted(&self) -> u64 {
        match self.mode {
            Mode::Failover => self.s.accepted[0],
            Mode::Dual => self.s.accepted[0] + self.s.accepted[1],
        }
    }

    pub fn rejected(&self) -> u64 {
        match self.mode {
            Mode::Failover => self.s.rejected[0],
            Mode::Dual => self.s.rejected[0] + self.s.rejected[1],
        }
    }

    pub fn best_session(&self) -> u64 {
        self.s.best_session[0].max(self.s.best_session[1])
    }

    /// Dual pool: the pool with the larger share of the balance.
    fn shown(&self) -> usize {
        match self.mode {
            Mode::Failover => 0,
            Mode::Dual if self.s.balance >= 50 => 0,
            Mode::Dual => 1,
        }
    }

    pub fn pool_difficulty(&self) -> u32 {
        self.s.pool_difficulty[self.shown()]
    }

    pub fn network_difficulty(&self) -> f64 {
        self.s.network_difficulty[self.shown()]
    }
}

pub(crate) mod ffi {
    use super::*;

    /// `pool_stats_t`
    pub struct Handle(Mutex<PoolStats>);

    impl Handle {
        pub(crate) fn with<T>(&self, f: impl FnOnce(&mut PoolStats) -> T) -> T {
            f(&mut self.0.lock().unwrap_or_else(|e| e.into_inner()))
        }
    }

    /// `pool_stats_summary_t`
    #[repr(C)]
    pub struct Summary {
        pub accepted: u64,
        pub rejected: u64,
        pub best_session: u64,
        pub total_best: u64,
        pub network_difficulty: f64,
        pub pool_difficulty: u32,
        pub found_blocks: u32,
        pub total_found_blocks: u32,
        pub last_response_us: u64,
    }

    /// `pool_stats_pool_t`
    #[repr(C)]
    pub struct PoolSummary {
        pub accepted: u64,
        pub rejected: u64,
        pub best_session: u64,
        pub network_difficulty: f64,
        pub pool_difficulty: u32,
    }

    /// `pool_stats_event_t`
    #[repr(C)]
    pub struct CEvent {
        pub new_total_best: bool,
        pub found_block: bool,
        pub total_best: u64,
        pub total_found_blocks: u32,
        pub network_difficulty: f64,
    }

    impl From<Event> for CEvent {
        fn from(e: Event) -> Self {
            CEvent {
                new_total_best: e.new_total_best,
                found_block: e.found_block,
                total_best: e.total_best,
                total_found_blocks: e.total_found_blocks,
                network_difficulty: e.network_difficulty,
            }
        }
    }

    pub extern "C" fn pool_stats_new(
        dual: bool,
        total_best: u64,
        total_found_blocks: u32,
    ) -> *mut Handle {
        let mode = if dual { Mode::Dual } else { Mode::Failover };
        Box::into_raw(Box::new(Handle(Mutex::new(PoolStats::new(
            mode,
            total_best,
            total_found_blocks,
        )))))
    }

    // All below: `h` from `pool_stats_new()`.

    /// # Safety
    /// See above.
    pub unsafe extern "C" fn pool_stats_set_totals(
        h: *const Handle,
        total_best: u64,
        total_found_blocks: u32,
    ) {
        (*h).with(|s| s.set_totals(total_best, total_found_blocks))
    }

    /// # Safety
    /// See above.
    pub unsafe extern "C" fn pool_stats_set_balance(h: *const Handle, balance: u16) {
        (*h).with(|s| s.set_balance(balance))
    }

    /// # Safety
    /// See above; `ev` writable.
    pub unsafe extern "C" fn pool_stats_best_diff(
        h: *const Handle,
        pool: i32,
        difficulty: f64,
        nbits: u32,
        ev: *mut CEvent,
    ) {
        *ev = (*h).with(|s| s.best_diff(pool, difficulty, nbits)).into();
    }

    /// # Safety
    /// See above; `ev` writable.
    pub unsafe extern "C" fn pool_stats_found_block(
        h: *const Handle,
        difficulty: f64,
        nbits: u32,
        ev: *mut CEvent,
    ) {
        *ev = (*h).with(|s| s.found_block(difficulty, nbits)).into();
    }

    /// # Safety
    /// See above.
    pub unsafe extern "C" fn pool_stats_reset_pool(h: *const Handle, pool: i32) {
        (*h).with(|s| s.reset_pool(pool))
    }

    /// # Safety
    /// See above.
    pub unsafe extern "C" fn pool_stats_reset_session(h: *const Handle) {
        (*h).with(PoolStats::reset_session)
    }

    /// # Safety
    /// See above.
    pub unsafe extern "C" fn pool_stats_reset_all_time(h: *const Handle) {
        (*h).with(PoolStats::reset_all_time)
    }

    /// One consistent snapshot of the aggregates.
    ///
    /// # Safety
    /// See above; `out` writable.
    #[no_mangle]
    pub unsafe extern "C" fn pool_stats_summary(h: *const Handle, out: *mut Summary) {
        *out = (*h).with(|s| Summary {
            accepted: s.accepted(),
            rejected: s.rejected(),
            best_session: s.best_session(),
            total_best: s.s.total_best,
            network_difficulty: s.network_difficulty(),
            pool_difficulty: s.pool_difficulty(),
            found_blocks: s.s.found_blocks,
            total_found_blocks: s.s.total_found_blocks,
            last_response_us: s.s.last_response_us,
        });
    }

    /// The counters of one pool (failover: the shared ones).
    ///
    /// # Safety
    /// See above; `out` writable.
    #[no_mangle]
    pub unsafe extern "C" fn pool_stats_pool(h: *const Handle, pool: i32, out: *mut PoolSummary) {
        *out = (*h).with(|s| {
            let i = s.slot(pool).unwrap_or(0);
            PoolSummary {
                accepted: s.s.accepted[i],
                rejected: s.s.rejected[i],
                best_session: s.s.best_session[i],
                network_difficulty: s.s.network_difficulty[i],
                pool_difficulty: s.s.pool_difficulty[i],
            }
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const NBITS: u32 = 0x1703_a30c;

    #[test]
    fn failover_shares_one_set_of_counters() {
        let mut p = PoolStats::new(Mode::Failover, 1000, 2);
        p.share_result(0, true, 5);
        p.share_result(1, true, 6);
        p.share_result(1, false, 7);
        p.set_pool_difficulty(1, 4096);
        p.set_network_difficulty(0, NBITS);
        p.set_network_difficulty(0, 0); // ignored
        assert_eq!(
            (p.accepted(), p.rejected(), p.pool_difficulty()),
            (2, 1, 4096)
        );
        assert!(p.network_difficulty() > 1e13);
        assert_eq!(p.stats().last_response_us, 7);
        p.reset_pool(1); // resets the shared counters
        assert_eq!((p.accepted(), p.rejected()), (0, 0));
    }

    #[test]
    fn dual_pool_counts_per_pool() {
        let mut p = PoolStats::new(Mode::Dual, 0, 0);
        assert_eq!(p.stats().balance, 50);
        p.share_result(0, true, 1);
        p.share_result(1, true, 2);
        p.share_result(1, true, 3);
        p.share_result(5, true, 4); // no such pool
        p.set_pool_difficulty(0, 100);
        p.set_pool_difficulty(1, 200);
        assert_eq!(p.accepted(), 3);
        p.set_balance(50);
        assert_eq!(p.pool_difficulty(), 100);
        p.set_balance(49);
        assert_eq!(p.pool_difficulty(), 200);
        p.reset_pool(1);
        assert_eq!((p.stats().accepted, p.accepted()), ([1, 0], 1));
    }

    #[test]
    fn best_and_found() {
        let mut p = PoolStats::new(Mode::Dual, 5000, 1);
        assert!(!p.best_diff(0, 4000.5, NBITS).new_total_best);
        assert_eq!(p.best_session(), 4000);
        let e = p.best_diff(1, 6000.9, NBITS);
        assert!(e.new_total_best && e.total_best == 6000);
        // out-of-range pool: nothing, like the former range check
        assert_eq!(p.best_diff(7, 1e9, NBITS), Event::default());
        assert!(!p.found_block(1e9, NBITS).found_block);
        let e = p.found_block(1e20, NBITS);
        assert!(e.found_block && e.total_found_blocks == 2);
        assert_eq!(p.stats().found_blocks, 1);
        p.reset_session();
        assert_eq!(
            (
                p.stats().found_blocks,
                p.best_session(),
                p.stats().total_best
            ),
            (0, 0, 6000)
        );
        p.reset_all_time();
        assert_eq!((p.stats().total_best, p.stats().total_found_blocks), (0, 0));
        // NaN and huge values saturate
        assert_eq!(p.best_diff(0, f64::NAN, NBITS), Event::default());
        assert_eq!(p.best_diff(0, 1e300, NBITS).total_best, u64::MAX);
    }
}
