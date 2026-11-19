/*
 * The work the miner hands to the ASICs (implemented in Rust,
 * components/rust_core/src/jobs.rs): per pool the latest work from the pool
 * (Stratum v1, SV2 standard or extended channel; fed by pool_dispatch), built
 * into bm_jobs on request, and the tables that map the job IDs sent to the
 * ASICs and CAN slaves back to their jobs. Thread-safe.
 */

#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>
#include <stdint.h>

/* a job for the ASICs (the former components/bm1397 mining.h) */
#ifndef BM_JOB_DEFINED
#define BM_JOB_DEFINED
typedef struct
{
    uint32_t version;
    uint32_t version_mask;
    uint8_t prev_block_hash[32];
    uint8_t prev_block_hash_be[32];
    uint8_t merkle_root[32];
    uint8_t merkle_root_be[32];
    uint32_t ntime;
    uint32_t target; // aka difficulty, aka nbits
    uint32_t starting_nonce;
    // real stratum pool difficulty
    uint32_t pool_diff;
    // effective asic difficulty, limited to the ASIC's range
    uint32_t asic_diff;
    int pool_id;
    char *jobid;
    char *extranonce2;
} bm_job;
#endif
#include "sv2_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The layout must match rust_core (src/jobs.rs). */
#if UINTPTR_MAX == 0xFFFFFFFFu
#define JOBS_SIZES_OK (sizeof(bm_job) == 168)
#else
#define JOBS_SIZES_OK (sizeof(bm_job) == 176)
#endif
#ifdef __cplusplus
static_assert(JOBS_SIZES_OK, "jobs.h layout must match rust_core");
#else
_Static_assert(JOBS_SIZES_OK, "jobs.h layout must match rust_core");
#endif

/* Job tables: the local ASICs and one per CAN slave (CAN_SLAVE_MAX = 32). */
#define MAX_ASIC_JOBS 128
#define ASIC_JOBS_MASTER 0
#define ASIC_JOBS_SLAVE(n) (1 + (n))

/* Takes over job (freed when replaced or cleaned); IDs >= MAX_ASIC_JOBS are
 * freed right away. */
void asic_jobs_store(int table, bm_job *job, uint8_t asic_job_id);
/* A copy of the job sent with asic_job_id (free with free_bm_job()), or NULL. */
bm_job *asic_jobs_clone(int table, uint8_t asic_job_id);
/* Drops the jobs of pool; returns how many. */
int asic_jobs_clean(int table, int pool);

/* The pool's work is no longer valid; its jobs are dropped. */
void job_source_invalidate(int pool);
/* An SV2 extended channel job (job stays with the caller). */
void job_source_sv2_extended(int pool, const sv2_ext_job_t *job, uint8_t extranonce_size, uint32_t version_mask,
                             uint32_t difficulty);

/*
 * The next job for pool (free with free_bm_job()), or NULL without valid
 * work. select_diff maps the pool difficulty to the ASIC difficulty; it is
 * called with the job sources locked. log_new_work: log when the work
 * changed since the last such call.
 */
bm_job *job_source_build(int pool, uint32_t extranonce2, bool log_new_work,
                         uint32_t (*select_diff)(void *ctx, int pool, uint32_t pool_diff), void *ctx);

/* Wakes the job creation task (create_jobs_task.cpp). */
void trigger_job_creation(void);

#ifdef __cplusplus
}
#endif

#endif /* JOBS_H */
