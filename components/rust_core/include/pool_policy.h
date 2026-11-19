/*
 * Pool selection and coinbase verification decisions of the stratum manager
 * (implemented in Rust, components/rust_core/src/pool.rs). The managers pass
 * the current state in and carry out the returned actions.
 */

#ifndef POOL_POLICY_H
#define POOL_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What the manager knows about both pools when an event happens. */
typedef struct {
    bool has_task[2];
    bool connected[2]; // isConnected(): the task exists and is connected
    bool verify_blocked[2];
} pool_state_t;

/* What the manager has to do, per pool; apply in this order. */
typedef struct {
    bool invalidate[2];          // create_job_invalidate() and clear the valid-notify flag
    bool disconnect[2];
    bool stop_timer[2];          // stop the reconnect timer
    bool start_timer[2];         // start the reconnect timer
    bool connect[2];
    bool reset_verify_counts[2]; // reset the verification counters
} pool_actions_t;

/* Why a pool is blocked by the coinbase verification. */
enum { POOL_BLOCK_NONE = 0, POOL_BLOCK_ADDRESS_NOT_FOUND = 1, POOL_BLOCK_FEE_EXCEEDED = 2 };

typedef struct {
    uint16_t mode;    // 0 off, 1 address must be paid, 2 also the fee below max_fee
    uint16_t max_fee; // in tenths of a percent
    bool force;       // block (and reconnect) the pool when the verification fails
    uint64_t user_value_satoshis;
    uint64_t total_value_satoshis;
    uint32_t block_height;
    bool was_ok;
    uint8_t blocked; // POOL_BLOCK_*
} pool_verify_input_t;

typedef struct {
    bool ok;
    bool counted;     // count the check (and a failure if !ok)
    bool alert;       // just turned from ok to failed: send the alert
    uint8_t blocked;  // new block reason, POOL_BLOCK_*
    bool reconnect;   // just got blocked: reconnect the pool
    bool check_fault; // re-evaluate whether any configured pool is usable
} pool_verify_result_t;

#ifdef __cplusplus
static_assert(sizeof(pool_state_t) == 6 && sizeof(pool_actions_t) == 12 && sizeof(pool_verify_input_t) == 32 &&
                  sizeof(pool_verify_result_t) == 6,
              "pool_policy.h layout must match rust_core");
#else
_Static_assert(sizeof(pool_state_t) == 6 && sizeof(pool_actions_t) == 12 && sizeof(pool_verify_input_t) == 32 &&
                   sizeof(pool_verify_result_t) == 6,
               "pool_policy.h layout must match rust_core");
#endif

/* The policy of a manager: failover or dual pool. Thread-safe. */
typedef struct pool_policy pool_policy_t;

pool_policy_t *pool_policy_new(bool dual);
void pool_policy_free(pool_policy_t *p);

pool_actions_t pool_policy_reconnect_timer(pool_policy_t *p, int pool, const pool_state_t *s);
pool_actions_t pool_policy_connected(pool_policy_t *p, int pool, const pool_state_t *s);
pool_actions_t pool_policy_disconnected(pool_policy_t *p, int pool, const pool_state_t *s);

/* Failover: the selected pool. Dual pool: the pool for the next job; validN:
 * the pool has valid work and is not verify-blocked. */
int pool_policy_next_pool(pool_policy_t *p, bool valid0, bool valid1);
bool pool_policy_accepts_notify_from(pool_policy_t *p, int pool);
uint32_t pool_policy_select_asic_diff(pool_policy_t *p, int pool, uint32_t pool_diff, uint32_t asic_min,
                                      uint32_t asic_max);

/* Dual pool only (failover: false / 0). */
bool pool_policy_set_balance(pool_policy_t *p, uint16_t balance); // true if changed
bool pool_policy_pool_diff_err(pool_policy_t *p, int pool);
int pool_policy_active_balance(pool_policy_t *p, int pool, const pool_state_t *s);

/* The pool a per-pool view shows for pool (failover: the selected one). */
int pool_policy_display_pool(pool_policy_t *p, int pool);
/* The pool whose ping the legacy single-pool getters report (failover: the
 * selected one; dual pool: the one with the larger balance share). */
int pool_policy_compat_ping_pool(pool_policy_t *p);

pool_verify_result_t pool_verify(const pool_verify_input_t *input);
bool pool_any_usable(bool configured0, bool configured1, bool blocked0, bool blocked1);

#ifdef __cplusplus
}
#endif

#endif /* POOL_POLICY_H */
