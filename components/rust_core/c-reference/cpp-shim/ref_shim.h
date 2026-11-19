/* Force-included into the reference (old) build of stratum_api.cpp only:
 * strtoul() with newlib's 32-bit unsigned long semantics, as on the ESP32.
 * Every header that might use strtoul itself is included before the macro. */
#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include "ArduinoJson.h"
extern "C" unsigned long strtoul_newlib32(const char *s, char **end, int base);
#define strtoul strtoul_newlib32
