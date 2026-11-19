/* host build: the fan settings of the former Config namespace, from a table
 * set by the differential test */
#pragma once
#include <stdint.h>

struct FanCfgStub {
    uint16_t manual[2];
    uint16_t overheat[2];
    bool use_max;
};
extern FanCfgStub g_fan_cfg;

namespace Config {
inline uint16_t getFanManualSpeed(int ch) { return g_fan_cfg.manual[ch]; }
inline uint16_t getFanOverheatTemp(int ch) { return g_fan_cfg.overheat[ch]; }
inline uint16_t getFanPidTargetTemp(int, uint16_t d) { return d; }
inline uint16_t getFanPidP(int, uint16_t d) { return d; }
inline uint16_t getFanPidI(int, uint16_t d) { return d; }
inline uint16_t getFanPidD(int, uint16_t d) { return d; }
inline bool isFanPidUseMax() { return g_fan_cfg.use_max; }
} // namespace Config
