/* host build: FreeRTOS ticks (1 tick = 1 ms) that advance with vTaskDelay() */
#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
#endif
#define pdTICKS_TO_MS(t) ((uint32_t) (t))
#ifdef __cplusplus
extern "C" {
#endif
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
#ifdef __cplusplus
}
#endif
