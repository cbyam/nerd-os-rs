#pragma once
#include <stdint.h>

#include "hashrate_monitor.h"

// measurement period of the hashrate monitor (hashrate.rs) and the history
#define HR_INTERVAL 5000


// The hashrate from the chips' counters; the measurement and the rates are
// in Rust (hashrate_monitor.h), this class starts the task.
class HashrateMonitor {
  public:
    // Starts the measurement task (counters read every 5 s; Rust, tasks.rs).
    bool start() { return hashrate_monitor_start(); }

    // Called from the RX dispatcher for each 0x90 / 0x4C register reply.
    void onRegisterReply(uint8_t asic_idx, uint32_t counterNow) { hashrate_monitor_on_counter(asic_idx, counterNow); }
    void onErrorReply(uint8_t asic_idx, uint32_t counterNow) { hashrate_monitor_on_errors(asic_idx, counterNow); }

    // Per-chip accessors for the v2 dashboard.
    float getChipHashrate(int nr) { return hashrate_monitor_chip(nr); }
    float getChipErrorRate(int nr) { return hashrate_monitor_chip_errors(nr); }

    float getSmoothedTotalChipHashrate() { return hashrate_monitor_smoothed(); }
    float getHashrate() { return hashrate_monitor_total(); }

    // CAN slave hashrate accumulator; the master sets it whenever telemetry
    // arrives from the slaves.
    void setExternalHashrate(float ghs) { hashrate_monitor_set_external(ghs); }
    float getExternalHashrate() { return hashrate_monitor_external(); }
};
