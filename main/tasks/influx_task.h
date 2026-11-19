#pragma once

// samples and the record are in Rust (influx_rs.h)
#include "influx_rs.h"

// the task (Rust, components/rust_core/src/tasks.rs)
extern "C" void influx_task(void *pvParameters);
