/*
 * Mining statistics of the pool manager (implemented in Rust,
 * components/rust_core/src/pool_stats.rs): shares, difficulties, best
 * difficulty, found blocks. Thread-safe: every call locks one mutex.
 * Failover mode shares one set of counters for both pools; dual pool mode
 * counts per pool.
 */

#ifndef POOL_STATS_H
#define POOL_STATS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pool_stats pool_stats_t;

/* What to do after a nonce: persist and alert. */
typedef struct {
    bool new_total_best;  /* persist total_best, best difficulty alert */
    bool found_block;     /* persist total_found_blocks, block found alert */
    uint64_t total_best;
    uint32_t total_found_blocks;
    double network_difficulty; /* of the nonce's job */
} pool_stats_event_t;

/* Aggregates as the manager reported them (dual pool: sums, the pool with
 * the larger balance share for the difficulties, the larger session best). */
typedef struct {
    uint64_t accepted;
    uint64_t rejected;
    uint64_t best_session;
    uint64_t total_best;
    double network_difficulty;
    uint32_t pool_difficulty;
    uint32_t found_blocks;
    uint32_t total_found_blocks;
    uint64_t last_response_us; /* esp_timer_get_time() of the last share response, 0: none */
} pool_stats_summary_t;

/* One pool (failover: the shared counters). */
typedef struct {
    uint64_t accepted;
    uint64_t rejected;
    uint64_t best_session;
    double network_difficulty;
    uint32_t pool_difficulty;
} pool_stats_pool_t;

/* total_best, total_found_blocks: the persisted all-time values. */
pool_stats_t *pool_stats_new(bool dual, uint64_t total_best, uint32_t total_found_blocks);
void pool_stats_free(pool_stats_t *stats);

void pool_stats_share_result(const pool_stats_t *stats, int pool, bool accepted, uint64_t now_us);
void pool_stats_set_pool_difficulty(const pool_stats_t *stats, int pool, uint32_t difficulty);
/* nbits 0 is ignored */
void pool_stats_set_network_difficulty(const pool_stats_t *stats, int pool, uint32_t nbits);
void pool_stats_set_balance(const pool_stats_t *stats, uint16_t balance);
/* the persisted all-time values, on loading the settings */
void pool_stats_set_totals(const pool_stats_t *stats, uint64_t total_best, uint32_t total_found_blocks);

/* A nonce: session best of the pool and the all-time best. */
void pool_stats_best_diff(const pool_stats_t *stats, int pool, double difficulty, uint32_t nbits,
                          pool_stats_event_t *ev);
/* A nonce above the network difficulty of nbits is a block. */
void pool_stats_found_block(const pool_stats_t *stats, double difficulty, uint32_t nbits, pool_stats_event_t *ev);

void pool_stats_reset_pool(const pool_stats_t *stats, int pool);
void pool_stats_reset_session(const pool_stats_t *stats); /* both pools and the found blocks */
void pool_stats_reset_all_time(const pool_stats_t *stats);

void pool_stats_summary(const pool_stats_t *stats, pool_stats_summary_t *out);
void pool_stats_pool(const pool_stats_t *stats, int pool, pool_stats_pool_t *out);

#ifdef __cplusplus
}
#endif

#endif /* POOL_STATS_H */
