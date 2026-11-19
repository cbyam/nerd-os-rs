/* C interface to the former ASIC driver classes (c-asic/, classes renamed to
 * Ref*) for the Rust tests, as ref_*. The same interface for the current
 * driver (new_*) is in asic_wrap_current.cpp. */
#include <cstdint>
#include <cstring>

#define protected public
#include "bm1366.h"
#include "bm1368.h"
#include "bm1370.h"
#include "bm1373.h"
#undef protected

#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#define F(name) CAT(PREFIX, name)

#define A(a) (static_cast<Asic *>(a))

extern "C" {
void *F(asic_new)(int chip)
{
    switch (chip) {
    case 0: return static_cast<Asic *>(new BM1366());
    case 1: return static_cast<Asic *>(new BM1368());
    case 2: return static_cast<Asic *>(new BM1370());
    default: return static_cast<Asic *>(new BM1373());
    }
}
void F(asic_free)(void *a) { delete A(a); }
void F(asic_send)(void *a, uint8_t header, const uint8_t *data, uint8_t len) { A(a)->send(header, (uint8_t *) data, len); }
uint8_t F(asic_send_work)(void *a, uint32_t job_id, uint32_t version, uint32_t target, uint32_t ntime,
                          uint32_t starting_nonce, const uint8_t *merkle_root_be, const uint8_t *prev_block_hash_be)
{
    bm_job job;
    memset(&job, 0, sizeof(job));
    job.version = version;
    job.target = target;
    job.ntime = ntime;
    job.starting_nonce = starting_nonce;
    memcpy(job.merkle_root_be, merkle_root_be, 32);
    memcpy(job.prev_block_hash_be, prev_block_hash_be, 32);
    return A(a)->sendWork(job_id, &job);
}
void F(asic_set_difficulty)(void *a, int difficulty) { A(a)->setJobDifficultyMask(difficulty); }
void F(asic_set_address_interval)(void *a, uint8_t interval) { A(a)->m_addressInterval = interval; }
bool F(asic_process)(void *a, task_result *result) { return A(a)->processWork(result); }
void F(asic_set_vr_frequency)(void *a, uint32_t f) { A(a)->setVrFrequency(f); }
uint32_t F(asic_default_vr_frequency)(void *a) { return A(a)->getDefaultVrFrequency(); }
uint8_t F(asic_init)(void *a, uint64_t frequency, uint16_t count, uint32_t difficulty, uint32_t vr)
{
    return A(a)->init(frequency, count, difficulty, vr);
}
bool F(asic_set_frequency)(void *a, float f) { return A(a)->setAsicFrequency(f); }
bool F(asic_send_hash_frequency)(void *a, float f, float *actual)
{
    bool ok = A(a)->sendHashFrequency(f);
    *actual = A(a)->m_actual_current_frequency;
    return ok;
}
int F(asic_set_max_baud)(void *a) { return A(a)->setMaxBaud(); }
void F(asic_request_temp)(void *a) { A(a)->requestChipTemp(); }
void F(asic_reset_counter)(void *a, uint8_t reg) { A(a)->resetCounter(reg); }
void F(asic_read_counter)(void *a, uint8_t reg) { A(a)->readCounter(reg); }
void F(asic_send_raw_job)(void *a, const uint8_t *job) { A(a)->sendRawJob((BM1368_job *) job); }
const char *F(asic_name)(void *a) { return A(a)->getName(); }
uint16_t F(asic_small_core_count)(void *a) { return A(a)->getSmallCoreCount(); }
}
