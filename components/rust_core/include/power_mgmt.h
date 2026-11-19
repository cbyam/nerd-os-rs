/*
 * The power management (implemented in Rust,
 * components/rust_core/src/power_mgmt.rs): the hardware lock, shutdown and
 * restart, the power task with the fan control, the values it measures.
 */

#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the task entry (xTaskCreatePSRAM) */
void pm_task(void *arg);
/* the hardware lock (recursive): ASICs and I2C devices */
void pm_lock(void);
void pm_unlock(void);
bool pm_is_shutdown(void);
void pm_shutdown(void);
/* shuts down under the lock and restarts; does not return */
void pm_restart(void);
/* 0 power, 1 voltage, 2 current, 3 chip temp, 4 VR temp, 5 VR temp internal */
float pm_get(int what);
/* 0 RPM, 1 %, 2 overheated, 3 overheat threshold */
uint16_t pm_fan_get(int ch, int what);
void pm_fan_load_settings(void);
bool pm_fan_pid_use_max(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MGMT_H */
