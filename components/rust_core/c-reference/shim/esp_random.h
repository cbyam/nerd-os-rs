#pragma once
#include <stddef.h>
/* Defined by rust_core's host stand-ins (host/esp_host.c); tests script it. */
void esp_fill_random(void *buf, size_t len);
