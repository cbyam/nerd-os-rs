/*
 * Hashrate and temperature history (implemented in Rust,
 * components/rust_core/src/history.rs): a day of 5 s samples, the 1 min /
 * 10 min / 1 h / 1 d sliding averages, the chart export and the share count
 * per chip. Thread-safe.
 */

#ifndef HASHRATE_HISTORY_H
#define HASHRATE_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One exported sample: averages and temperatures * 100, time relative to the
 * range start (ms). */
typedef struct {
    int hashrate_1m;
    int hashrate_10m;
    int hashrate_1h;
    int hashrate_1d;
    int vreg_temp;
    int asic_temp;
    int64_t timestamp;
} history_row_t;

#ifdef __cplusplus
static_assert(sizeof(history_row_t) == 32, "hashrate_history.h layout must match rust_core");
#else
_Static_assert(sizeof(history_row_t) == 32, "hashrate_history.h layout must match rust_core");
#endif

/* Allocates the buffers (about 1.2 MB, PSRAM); false without memory. */
bool history_init(int asic_count);
bool history_available(void);
/* A measured hashrate (GH/s) and the temperatures at timestamp (ms). */
void history_push(float rate, float vreg_temp, float asic_temp, uint64_t timestamp_ms);
void history_push_share(int asic);
/* The display average of window 0..3 (1m, 10m, 1h, 1d). */
double history_current(int window);
uint64_t history_current_timestamp(void);

/*
 * The samples between start and end (client time, ms; current: the client's
 * now, now_ms: ours), at most limit (0: no limit): row is called for each.
 * Returns true if the limit cut the range.
 */
bool history_export(uint64_t start, uint64_t end, uint64_t current, uint32_t limit, uint64_t now_ms,
                    void (*row)(void *ctx, const history_row_t *row), void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HASHRATE_HISTORY_H */
