/* The C interface of asic_wrap.cpp (new_*) for the current driver: the
 * Rust driver (asic_driver.h) the firmware uses, on the scripted UART
 * (serial_stub.cpp). */
#include <cstring>

#include "asic_driver.h"
#include "jobs.h"

extern "C" {
int serial_stub_send(uint8_t *data, int len);
int16_t serial_stub_rx(uint8_t *buf, uint16_t size);
void serial_stub_clear(void);
}

// as the firmware's UART (serial.rs): the length as a 16-bit count
static const asic_uart_t s_uart = {
    .ctx = nullptr,
    .send = [](void *, const uint8_t *data, size_t len) { serial_stub_send((uint8_t *) data, (int) len); },
    .rx = [](void *, uint8_t *buf, size_t len, uint16_t) { return (int) serial_stub_rx(buf, (uint16_t) len); },
    .clear = [](void *) { serial_stub_clear(); },
    .delay_ms = [](void *, uint32_t) {},
};

#define D(a) (static_cast<asic_driver_t *>(a))

extern "C" {
void *new_asic_new(int chip)
{
    // as the firmware's board: one of the four chips
    return asic_driver_new(chip >= 0 && chip <= 3 ? (asic_chip_t) chip : ASIC_BM1373, &s_uart);
}
void new_asic_free(void *a) { asic_driver_free(D(a)); }
void new_asic_send(void *a, uint8_t header, const uint8_t *data, uint8_t len)
{
    asic_driver_send_frame(D(a), header, data, len);
}
uint8_t new_asic_send_work(void *a, uint32_t job_id, uint32_t version, uint32_t target, uint32_t ntime,
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
    return asic_driver_send_work(D(a), job_id, job.starting_nonce, job.target, job.ntime, job.merkle_root_be,
                                 job.prev_block_hash_be, job.version);
}
void new_asic_set_difficulty(void *a, int difficulty) { asic_driver_set_difficulty_mask(D(a), difficulty); }
void new_asic_set_address_interval(void *a, uint8_t interval)
{
    asic_driver_set_address_interval(D(a), interval);
}
bool new_asic_process(void *a, task_result *result) { return asic_driver_process_work(D(a), result); }
void new_asic_set_vr_frequency(void *a, uint32_t f) { asic_driver_set_vr_frequency(D(a), f); }
uint32_t new_asic_default_vr_frequency(void *a) { return asic_driver_default_vr_frequency(D(a)); }
uint8_t new_asic_init(void *a, uint64_t frequency, uint16_t count, uint32_t difficulty, uint32_t vr)
{
    return asic_driver_init(D(a), frequency, count, difficulty, vr);
}
bool new_asic_set_frequency(void *a, float f) { return asic_driver_set_frequency(D(a), f); }
bool new_asic_send_hash_frequency(void *a, float f, float *actual)
{
    return asic_driver_send_hash_frequency(D(a), f, actual);
}
int new_asic_set_max_baud(void *a) { return asic_driver_set_max_baud(D(a)); }
void new_asic_request_temp(void *a) { asic_driver_request_chip_temp(D(a)); }
void new_asic_reset_counter(void *a, uint8_t reg) { asic_driver_reset_counter(D(a), reg); }
void new_asic_read_counter(void *a, uint8_t reg) { asic_driver_read_counter(D(a), reg); }
void new_asic_send_raw_job(void *a, const uint8_t *job) { asic_driver_send_raw_job(D(a), job); }
const char *new_asic_name(void *a) { return asic_driver_name(D(a)); }
uint16_t new_asic_small_core_count(void *a) { return asic_driver_small_core_count(D(a)); }
}
