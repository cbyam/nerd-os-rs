#pragma once

// The CAN bus (Rust, components/rust_core/src/can.rs): the TWAI driver at
// 500 kbit/s, the master's receive task and the slave's tasks.

#ifdef __cplusplus
extern "C" {
#endif

void can_init(int tx_gpio, int rx_gpio);
void can_master_task(void *arg);
void can_slave_task(void *arg);
void can_slave_result_task(void *arg);
void can_slave_telemetry_task(void *arg);

#ifdef __cplusplus
}
#endif
