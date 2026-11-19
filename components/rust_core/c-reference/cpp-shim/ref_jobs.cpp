/* The former Stratum v1 job construction for the Rust tests. The body of
 * ref_build_job() is a verbatim copy of MiningInfoV1::buildBmJob() from
 * main/tasks/create_jobs_task.cpp before the port (commit 7d8a1add), with the
 * class members turned into parameters. */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "macros.h"
#include "mining.h"

extern "C" bool ref_build_job(mining_notify *current_job, const char *extranonce_str, int extranonce_2_len,
                              uint32_t extranonce_2, uint32_t version_mask, bm_job *out, char *out_extranonce2,
                              size_t out_extranonce2_len)
{
    uint32_t active_stratum_difficulty = 0;
    int pool_id = 0;
    uint32_t asic_diff = 0;

    // ---- verbatim from MiningInfoV1::buildBmJob() ----
        // generate extranonce2 hex string
        char extranonce_2_str[extranonce_2_len * 2 + 1]; // +1 zero termination
        snprintf(extranonce_2_str, sizeof(extranonce_2_str), "%0*lx", (int) extranonce_2_len * 2, (unsigned long) extranonce_2);

        // generate coinbase tx
        int coinbase_tx_len = strlen(current_job->coinbase_1) + strlen(extranonce_str) + strlen(extranonce_2_str) +
                              strlen(current_job->coinbase_2);
        char *coinbase_tx = (char *) MALLOC(coinbase_tx_len + 1);
        snprintf(coinbase_tx, coinbase_tx_len + 1, "%s%s%s%s", current_job->coinbase_1, extranonce_str,
                 extranonce_2_str, current_job->coinbase_2);

        // calculate merkle root
        char merkle_root[65];
        calculate_merkle_root_hash(coinbase_tx, current_job->_merkle_branches, current_job->n_merkle_branches,
                                   merkle_root);
        free(coinbase_tx);

        // we need malloc because we will save it in the job array
        bm_job *next_job = (bm_job *) MALLOC(sizeof(bm_job));
        construct_bm_job(current_job, merkle_root, version_mask, next_job);
        next_job->jobid = strdup(current_job->job_id);
        next_job->extranonce2 = strdup(extranonce_2_str);
        next_job->pool_diff = active_stratum_difficulty;
        next_job->pool_id = pool_id;
        next_job->asic_diff = asic_diff;
    // ---- end of copy ----

    *out = *next_job;
    out->jobid = nullptr;
    out->extranonce2 = nullptr;
    snprintf(out_extranonce2, out_extranonce2_len, "%s", next_job->extranonce2);
    free_bm_job(next_job);
    return true;
}

extern "C" double ref_test_nonce_value(const bm_job *job, uint32_t nonce, uint32_t rolled_version)
{
    return test_nonce_value(job, nonce, rolled_version);
}
