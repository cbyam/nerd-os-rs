/* Host stand-ins for the FreeRTOS calls of the C++ code under test. */
#pragma once
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif
static inline void vTaskDelay(int ticks)
{
    (void) ticks;
}
