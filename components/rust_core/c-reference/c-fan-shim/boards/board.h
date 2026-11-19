/* host build: the part of the Board class the former FanController uses;
 * fan accesses are recorded */
#pragma once
#include <stdint.h>
#include <string>
#include "pid/PID_v1_bc.h"

extern std::string g_fan_trace;

class Board {
  public:
    int numFans = 1;
    int fanMode[2] = {2, 3};
    PidSettings pid[2] = {{60, 600, 10, 1000}, {65, 600, 10, 1000}};
    uint16_t rpm[2] = {0, 0};

    int getNumFans() { return numFans; }
    PidSettings *getPidSettings(int ch = 0) { return &pid[ch]; }
    int getFanMode(int ch) { return fanMode[ch]; }
    void getFanSpeedCh(int ch, uint16_t *r);
    void setFanSpeedCh(int ch, float perc);
};
