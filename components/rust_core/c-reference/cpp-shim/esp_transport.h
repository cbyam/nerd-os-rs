#pragma once
typedef struct esp_transport_item_t *esp_transport_handle_t;
typedef struct {
    int unused;
} esp_transport_keep_alive_t;
/* Defined by rust_core's host stand-ins (host/esp_host.c); tests script them. */
#ifdef __cplusplus
extern "C" {
#endif
int esp_transport_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms);
int esp_transport_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms);
#ifdef __cplusplus
}
#endif
