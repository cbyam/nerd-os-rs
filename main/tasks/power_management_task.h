#pragma once

#include <stdint.h>

#include "fan_controller.h"
#include "power_mgmt.h"

template <class T>
class LockGuard {
public:
    LockGuard(T& obj) : m_obj(obj) { m_obj.lock(); }
    ~LockGuard() { m_obj.unlock(); }
private:
    T& m_obj;
};

// The power management: in Rust (power_mgmt.h, power_mgmt.rs, power_task.rs);
// this facade keeps the former accessors of POWER_MANAGEMENT_MODULE.
class PowerManagementTask {
  protected:
    FanController m_fanController;

  public:
    // synchronized rebooting to not mess up i2c comms
    void restart() { pm_restart(); }

    static void taskWrapper(void *pvParameters) { pm_task(pvParameters); }

    float getPower() { return pm_get(0); }
    float getVoltage() { return pm_get(1); }
    float getCurrent() { return pm_get(2); }
    float getChipTempMax() { return pm_get(3); }
    float getVRTemp() { return pm_get(4); }
    float getVRTempInt() { return pm_get(5); }

    uint16_t getFanRPM(int channel) { return m_fanController.getRPM(channel); }
    uint16_t getFanPerc(int ch = 0) { return m_fanController.getSpeedPerc(ch); }
    FanController& getFanController() { return m_fanController; }

    void lock() { pm_lock(); }
    void unlock() { pm_unlock(); }

    void shutdown() { pm_shutdown(); }
    bool isShutdown() { return pm_is_shutdown(); }
};
