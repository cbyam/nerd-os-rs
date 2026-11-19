#pragma once

#include <stdint.h>

#include "power_mgmt.h"

// The fan control of the power management: in Rust (power_mgmt.h,
// fan_control.rs); this facade keeps the former accessors.
class FanController {
  public:
    static constexpr int MAX_FANS = 2;

    // Reload all channel configs from NVS (the PID keeps its state).
    void loadSettings() { pm_fan_load_settings(); }
    uint16_t getRPM(int ch) const { return pm_fan_get(ch, 0); }
    uint16_t getSpeedPerc(int ch) const { return pm_fan_get(ch, 1); }
    bool isOverheated(int ch) const { return pm_fan_get(ch, 2) != 0; }
    uint16_t getOverheatTemp(int ch) const { return pm_fan_get(ch, 3); }
    bool isPidUseMax() const { return pm_fan_pid_use_max(); }
};
