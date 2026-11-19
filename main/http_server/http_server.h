#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Starts the HTTP server (Rust, components/rust_core/src/http_server.rs)
esp_err_t start_rest_server(void *pvParameters);

#ifdef __cplusplus
}
#endif
