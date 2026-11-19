/*
 * InfluxDB v2 telemetry (implemented in Rust, components/rust_core/src/influx.rs):
 * the power, temperature and fan samples are averaged between writes; every
 * 15 s one line-protocol record is written, after the bucket is checked (or
 * created) and the last totals are loaded. The HTTP requests are a hook.
 */

#ifndef INFLUX_RS_H
#define INFLUX_RS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The values taken from the system and the pool manager for each record. */
typedef struct {
    float hashrate;
    float hashrate_1m;
    uint64_t best_session_diff;
    uint64_t best_diff;
    uint64_t accepted;
    uint64_t rejected;
    uint64_t duplicates;
    int pool_errors;
    uint32_t pool_difficulty;
    uint32_t found_blocks;
    double last_ping_rtt;
    double recent_ping_loss;
} influx_inputs_t;

typedef struct {
    void *ctx;
    void (*suspend_if_shutdown)(void *ctx);
    void (*inputs)(void *ctx, influx_inputs_t *out);
    /* method 0 GET, 1 POST; header values NULL if not set; the answer into
     * resp (at most resp_cap bytes, *resp_len); the HTTP status, or < 0 if
     * there was no answer (the hook logs why) */
    int (*request)(void *ctx, int method, const char *url, const char *auth, const char *content_type,
                   const char *accept, const uint8_t *body, size_t body_len, uint8_t *resp, size_t resp_cap,
                   size_t *resp_len);
} influx_hooks_t;

/* The task; never returns. The strings are copied. */
void influx_run(bool enabled, const char *url, uint16_t port, const char *token, const char *bucket, const char *org,
                const char *prefix, const influx_hooks_t *hooks) __attribute__((noreturn));

/* Telemetry samples (power management task), averaged per record. */
void influx_task_set_temperature(float temp, float temp2);
void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout);
void influx_set_fan(float pwm0, float rpm0, float pwm1, float rpm1);

#ifdef __cplusplus
}
#endif

#endif /* INFLUX_RS_H */
