/*
 * Firmware and web UI updates over HTTP (implemented in Rust,
 * components/rust_core/src/http_ota.rs): receiving an upload into flash,
 * the body and URL check of the factory update. Flash, OTA and HTTP client
 * calls stay in C++.
 */

#ifndef HTTP_OTA_H
#define HTTP_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_UPLOAD_OK = 0,
    OTA_UPLOAD_TOO_LARGE = 1,
    OTA_UPLOAD_RECEIVE_ERROR = 2, /* connection closed or failed */
    OTA_UPLOAD_STALLED = 3,       /* 12 receive timeouts in a row */
    OTA_UPLOAD_WRITE_ERROR = 4,
} ota_upload_result_t;

/* recv: httpd_req_recv() (bytes, HTTPD_SOCK_ERR_TIMEOUT, or <= 0 on an
 * error); write: the bytes at offset into flash, false on an error. */
typedef struct {
    void *ctx;
    int (*recv)(void *ctx, uint8_t *buf, size_t len);
    bool (*write)(void *ctx, uint32_t offset, const uint8_t *data, size_t len);
} ota_upload_io_t;

/* Receives content_len bytes (at most max_len) through buf and writes them
 * in order; returns an ota_upload_result_t. */
int ota_receive_upload(const ota_upload_io_t *io, size_t content_len, size_t max_len, uint8_t *buf, size_t buf_len);

/* POST /api/system/OTA/github body {"url": "...", "keep_config": bool}:
 * 0 ok (url copied, *keep_config set; true unless false), 1 invalid JSON,
 * 2 no url string, 3 url longer than url_cap - 1. */
int ota_factory_body(const char *body, size_t len, char *url, size_t url_cap, bool *keep_config);

/* The factory image URL: under https://github.com/shufps/, no ".." steps
 * (also percent-encoded), no backslashes or control characters. */
bool ota_factory_url_ok(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_OTA_H */
