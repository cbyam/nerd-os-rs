/*
 * The stratum task loop shared by v1 and v2 (implemented in Rust,
 * components/rust_core/src/stratum_task.rs): load the config, wait for a
 * host, the network and the stop flag, resolve, connect, run the protocol
 * loop, tear down, wait before the next attempt.
 */

#ifndef STRATUM_TASK_LOOP_H
#define STRATUM_TASK_LOOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *ctx;
    const char *tag; // log tag (static string)
    void (*suspend_if_shutdown)(void *ctx);
    void (*clear_reconnect)(void *ctx);
    bool (*reconnect_requested)(void *ctx);
    // loads the config; returns the host (valid until the next call) and sets *port
    const char *(*load_config)(void *ctx, uint16_t *port);
    bool (*stopped)(void *ctx);
    bool (*network_up)(void *ctx);
    // DNS lookup; writes the address (NUL-terminated) into ip
    bool (*resolve)(void *ctx, const char *host, char *ip, size_t ip_len);
    // selects the transport and connects
    bool (*connect)(void *ctx, const char *host, const char *ip, uint16_t port);
    void (*protocol_loop)(void *ctx);
    void (*count_pool_error)(void *ctx);
    // marks the task disconnected, closes the transport, tells the manager
    void (*disconnect)(void *ctx);
    void (*delay_ms)(void *ctx, uint32_t ms);
} stratum_task_hooks_t;

/* Runs the task loop forever. */
void stratum_task_run(const stratum_task_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif /* STRATUM_TASK_LOOP_H */
