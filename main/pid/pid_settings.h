#pragma once

#include <stdint.h>

// Fan PID settings per channel (the PID itself is in Rust, fan_control.rs).
// we need packed here (for memcmp)
struct __attribute__((packed)) PidSettings {
    uint16_t targetTemp;
    uint16_t p;
    uint16_t i;
    uint16_t d;
};
