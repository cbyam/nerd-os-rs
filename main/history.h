#pragma once

#include "hashrate_history.h"

// The hashrate history; samples, averages and the export are in Rust
// (hashrate_history.h, the JSON in http_json.h).
class History {
  public:
    bool init(int numAsics) { return history_init(numAsics); }
    bool isAvailable() { return history_available(); }
    void pushShare(int asic_nr) { history_push_share(asic_nr); }
    void push(float rateGh, float vregTemp, float asicTemp, uint64_t timestamp)
    {
        history_push(rateGh, vregTemp, asicTemp, timestamp);
    }

    uint64_t getCurrentTimestamp(void) { return history_current_timestamp(); }
    double getCurrentHashrate1m() { return history_current(0); }
    double getCurrentHashrate10m() { return history_current(1); }
    double getCurrentHashrate1h() { return history_current(2); }
    double getCurrentHashrate1d() { return history_current(3); }

};
