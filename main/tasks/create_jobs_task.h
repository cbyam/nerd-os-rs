#pragma once

#include "jobs.h"

// Main task entry point: builds jobs from the pools' work (jobs.h) and sends
// them to the ASICs and CAN slaves (Rust, components/rust_core/src/tasks.rs)
extern "C" void create_jobs_task(void *pvParameters);
