#pragma once

// the network health log every 5 minutes (Rust, components/rust_core/src/wifi_health.rs)
extern "C" void wifi_monitor_task(void *arg);
