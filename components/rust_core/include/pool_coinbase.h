/*
 * Coinbase state of the pool manager (implemented in Rust,
 * components/rust_core/src/pool_coinbase.rs): per pool the extranonce, the
 * last decoded coinbase, the verification counters and the block reason.
 * Thread-safe. The caller sends the alert, reconnects and sets the board
 * fault flag as the returned outcome says.
 */

#ifndef POOL_COINBASE_H
#define POOL_COINBASE_H

#include <stdbool.h>
#include <stdint.h>

#include "coinbase_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pool_coinbase pool_coinbase_t;

/* The verification settings of a pool (from the config). */
typedef struct {
    uint16_t mode;    /* 0 off, 1 address paid, 2 also pool fee <= max_fee */
    uint16_t max_fee; /* tenths of a percent */
    bool force;       /* block and reconnect a failing pool */
} pool_verify_config_t;

typedef struct {
    bool verified;     /* the verification ran */
    bool alert;        /* send the verification failed alert */
    bool reconnect;    /* the pool just got blocked: reconnect it */
    bool check_fault;  /* re-evaluate whether any configured pool is usable */
    bool parse_failed; /* the coinbase could not be decoded */
} pool_verify_outcome_t;

typedef struct {
    coinbase_result_t result; /* the last decoded coinbase */
    bool ok;
    uint8_t blocked; /* POOL_BLOCK_* (pool_policy.h) */
    uint32_t check_count;
    uint32_t fail_count;
} pool_coinbase_info_t;

pool_coinbase_t *pool_coinbase_new(void);
void pool_coinbase_free(pool_coinbase_t *cb);

/* From the subscribe result or mining.set_extranonce (NULL clears it). */
void pool_coinbase_set_extranonce(const pool_coinbase_t *cb, int pool, const char *extranonce1, int extranonce2_len);

/* Decode and verify a coinbase: v1 with the stored extranonce (nothing
 * without one), v2 with the channel's. user: the pool user (address.worker). */
void pool_coinbase_process_v1(const pool_coinbase_t *cb, int pool, const char *coinbase_1, const char *coinbase_2,
                              uint32_t version, uint32_t nbits, const char *user, const pool_verify_config_t *config,
                              pool_verify_outcome_t *out);
void pool_coinbase_process_v2(const pool_coinbase_t *cb, int pool, const char *coinbase_1, const char *coinbase_2,
                              uint32_t version, uint32_t nbits, const char *extranonce1, int extranonce2_len,
                              const char *user, const pool_verify_config_t *config, pool_verify_outcome_t *out);
/* Verify the stored coinbase again (settings changed). */
void pool_coinbase_verify(const pool_coinbase_t *cb, int pool, const pool_verify_config_t *config,
                          pool_verify_outcome_t *out);

void pool_coinbase_reset_counts(const pool_coinbase_t *cb, int pool);
void pool_coinbase_clear_blocked(const pool_coinbase_t *cb, int pool);
uint8_t pool_coinbase_blocked(const pool_coinbase_t *cb, int pool); /* POOL_BLOCK_* */
void pool_coinbase_info(const pool_coinbase_t *cb, int pool, pool_coinbase_info_t *out);

#ifdef __cplusplus
}
#endif

#endif /* POOL_COINBASE_H */
