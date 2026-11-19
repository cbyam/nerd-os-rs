#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "jobs.h"
#include "tasks/power_management_task.h"
#include "stratum/stratum_manager.h"
#include "stratum/stratum_manager.h"
#include "apis_fetcher.h"
#include "network/network_manager.h"

#include "boards/board.h"
#include "system.h"
#include "discord.h"
#include "hashrate_monitor_task.h"
#include "otp_device.h"

extern System SYSTEM_MODULE;
extern PowerManagementTask POWER_MANAGEMENT_MODULE;
extern HashrateMonitor HASHRATE_MONITOR;

extern StratumManager *STRATUM_MANAGER;

// Rust (src/clock.rs)
extern "C" {
uint64_t now_ms(void);
uint32_t now(void);
bool is_time_synced(void);
void clock_sntp_start(void);
}
