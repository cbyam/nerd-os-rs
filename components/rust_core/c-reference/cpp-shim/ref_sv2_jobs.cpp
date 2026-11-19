/* The former SV2 job construction (c-sv2/mining_info_v2.cpp) for the Rust
 * tests: builds one job with each MiningInfoV2 class. */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mining_info_v2.h"

// Defined in main/tasks/create_jobs_task.cpp in the firmware.
MiningInfoBase::~MiningInfoBase() {}

static void take(bm_job *j, bm_job *out, char *extranonce2, size_t extranonce2_len)
{
    *out = *j;
    out->jobid = nullptr;
    out->extranonce2 = nullptr;
    if (extranonce2) {
        snprintf(extranonce2, extranonce2_len, "%s", j->extranonce2);
    }
    free(j->jobid);
    free(j->extranonce2);
    free(j);
}

extern "C" void ref_sv2_standard_job(const uint8_t merkle_root[32], const uint8_t prev_hash[32], bm_job *out)
{
    MiningInfoV2Standard m;
    m.updateJob(1, 0x20000000, merkle_root, prev_hash, 1000, 0x17034219, 0x1fffe000, 1);
    take(m.buildBmJob(0, 0, 1), out, nullptr, 0);
}

extern "C" void ref_sv2_extended_job(const sv2_ext_job_t *job, const uint8_t *extranonce_prefix,
                                     uint8_t extranonce_prefix_len, uint8_t extranonce_size, uint32_t counter,
                                     bm_job *out, char *extranonce2, size_t extranonce2_len)
{
    MiningInfoV2Extended m;
    m.updateJob(job, extranonce_prefix, extranonce_prefix_len, extranonce_size, 0x1fffe000, 1);
    take(m.buildBmJob(counter, 0, 1), out, extranonce2, extranonce2_len);
}
