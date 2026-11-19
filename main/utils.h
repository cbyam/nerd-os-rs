#pragma once

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Rust (src/suffix.rs): the value with an SI suffix (k, M, G, ...)
void suffixString(uint64_t val, char *buf, size_t bufSize, int sigDigits);

// Rust (src/idf/task.rs): a task with its stack in PSRAM
BaseType_t xTaskCreatePSRAM(TaskFunction_t pxTaskCode, const char *const pcName, const uint32_t usStackDepthBytes,
                            void *const pvParameters, UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask);

#ifdef __cplusplus
}
#endif
