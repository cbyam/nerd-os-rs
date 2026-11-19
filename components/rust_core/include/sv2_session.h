/*
 * Stratum v2 session state and job construction (implemented in Rust,
 * components/rust_core/src/stratum_v2/session.rs and jobs.rs).
 *
 * The connection loop (StratumTaskV2) feeds received frames into the session
 * and turns the resulting events into calls to the pool manager and the job
 * pipeline. The session is thread-safe: the connection task and the
 * share-submitting task share it.
 */

#ifndef SV2_SESSION_H
#define SV2_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mining_rs.h"
#include "sv2_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SV2_EVENT_NETWORK_DIFFICULTY, // value: nbits of a new prevhash
    SV2_EVENT_POOL_DIFFICULTY,    // value: pool difficulty of the opened channel
    SV2_EVENT_SET_TARGET,         // value: pool difficulty from SetTarget
    SV2_EVENT_STANDARD_JOB,       // job, value: pool difficulty
    SV2_EVENT_EXTENDED_JOB,       // ext_job (owned by the receiver), extranonce_size, value: pool difficulty
    SV2_EVENT_SHARES_ACCEPTED,    // value: number of shares
    SV2_EVENT_SHARE_REJECTED,     // error: error code
    SV2_EVENT_SETUP_FAILED,       // the connection is unusable (e.g. no version rolling)
    SV2_EVENT_VALID_NOTIFY,       // a prevhash arrived: the pool sends valid work
} sv2_event_kind_t;

typedef struct {
    sv2_event_kind_t kind;
    uint32_t value;
    sv2_job_t job;
    sv2_ext_job_t *ext_job; // free with sv2_ext_job_free()
    uint8_t extranonce_size;
    char error[64];
} sv2_event_t;

/* The layout must match rust_core (src/stratum_v2/session.rs). */
#if UINTPTR_MAX == 0xFFFFFFFFu
#define SV2_SESSION_SIZES_OK (sizeof(sv2_job_t) == 84 && sizeof(sv2_event_t) == 164)
#else
#define SV2_SESSION_SIZES_OK (sizeof(sv2_job_t) == 84 && sizeof(sv2_event_t) == 176)
#endif
#ifdef __cplusplus
static_assert(SV2_SESSION_SIZES_OK, "sv2_session.h layout must match rust_core");
#else
_Static_assert(SV2_SESSION_SIZES_OK, "sv2_session.h layout must match rust_core");
#endif

typedef struct sv2_session sv2_session_t;

/* A session for one connection (extended or standard channel). */
sv2_session_t *sv2_session_new(bool extended);
void sv2_session_free(sv2_session_t *session);
/* Starts over for a new connection; pending jobs and events are dropped. The
 * handle stays valid, so a concurrent submit never sees a freed session. */
void sv2_session_reset(sv2_session_t *session, bool extended);

/* Checks the answer to SetupConnection; false: the connection is unusable. */
bool sv2_session_setup_success(sv2_session_t *session, uint8_t msg_type, const uint8_t *payload, uint32_t len);

/* Checks and applies the answer to OpenStandard/ExtendedMiningChannel (an
 * extended channel's extranonce_size must be 1..32); queues the pool
 * difficulty. false: the connection is unusable. */
bool sv2_session_open_channel_success(sv2_session_t *session, uint8_t msg_type, const uint8_t *payload, uint32_t len);

/* Processes a frame from the main loop; may queue events. */
void sv2_session_handle(sv2_session_t *session, uint8_t msg_type, const uint8_t *payload, uint32_t len);

/* Takes the next event; false if there is none. */
bool sv2_session_next_event(sv2_session_t *session, sv2_event_t *ev);

/* Builds the SubmitShares frame for a share of a job of this session into
 * buf; returns its length or -1. job_id is the bm_job's decimal job id,
 * extranonce2_hex its extranonce2 (empty for standard jobs). */
int sv2_session_submit(sv2_session_t *session, uint8_t *buf, size_t buf_len, const char *job_id,
                       const char *extranonce2_hex, uint32_t ntime, uint32_t nonce, uint32_t version);

/* Standard channel job: the bm_job hash fields from the pool's merkle root
 * and prevhash (internal byte order). */
void sv2_job_hashes(const uint8_t merkle_root[32], const uint8_t prev_hash[32], mining_job_hashes_t *out);

/* Extended channel job: coinbase = prefix | extranonce prefix | extranonce2 |
 * suffix, merkle root, hash fields; extranonce2 (big-endian counter,
 * extranonce_size bytes) as hex into extranonce2_hex (65 bytes). false if
 * extranonce_size is larger than 32. */
bool sv2_build_extended_job(const uint8_t *coinbase_prefix, uint16_t coinbase_prefix_len,
                            const uint8_t *extranonce_prefix, uint8_t extranonce_prefix_len, uint8_t extranonce_size,
                            const uint8_t *coinbase_suffix, uint16_t coinbase_suffix_len,
                            const uint8_t (*merkle_path)[32], uint8_t merkle_path_count, const uint8_t prev_hash[32],
                            uint32_t extranonce2_counter, mining_job_hashes_t *out, char *extranonce2_hex);

/*
 * The connection loop (StratumTaskV2::protocolLoop), run on an established
 * Noise connection: SetupConnection, OpenChannel, then frames into the
 * session until the connection ends. The task, the manager and the session's
 * events are reached through sv2_hooks_t.
 */
typedef struct {
    void *ctx;
    // the current Noise context (sv2_noise_ctx_t *) and transport handle
    // (esp_transport_handle_t); false if the connection is gone
    bool (*link)(void *ctx, void **noise, void **transport);
    bool (*exit_requested)(void *ctx); // stop flag, reconnect request or power shutdown
    void (*connected)(void *ctx);      // the channel is open
    void (*drain_events)(void *ctx);   // hand the session's events on (sv2_session_next_event)
    bool (*setup_failed)(void *ctx);   // an event made the connection unusable
} sv2_hooks_t;

typedef struct {
    const char *host;
    uint16_t port;
    const char *vendor;
    const char *hw_version;
    const char *user;
    bool extended;
} sv2_setup_t;

typedef enum {
    SV2_EXIT_SETUP_FAILED,
    SV2_EXIT_OPEN_CHANNEL_FAILED,
    SV2_EXIT_EXIT_REQUESTED,
    SV2_EXIT_RECEIVE_FAILED,
    SV2_EXIT_CONNECTION_UNUSABLE,
} sv2_exit_t;

/* Runs the connection loop until the connection ends; resets the session
 * first. Returns an sv2_exit_t. */
int sv2_run(const sv2_hooks_t *hooks, const sv2_setup_t *setup, sv2_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* SV2_SESSION_H */
