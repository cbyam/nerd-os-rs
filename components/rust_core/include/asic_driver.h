/*
 * BM13xx chain driver (implemented in Rust, components/rust_core/src/asic):
 * init sequences, chip enumeration, jobs, responses, ticket mask, PLL and
 * frequency ramp, version rolling frequency. The UART is serial.rs (the
 * host tests: a scripted stub), reached through asic_uart_t.
 *
 * One driver may be used by several tasks at the same time (jobs, results,
 * counters, frequency changes); asic_driver_process_work() blocks up to 60 s
 * in the receive without blocking the others.
 */

#ifndef ASIC_DRIVER_H
#define ASIC_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASIC_BM1366 = 0,
    ASIC_BM1368 = 1,
    ASIC_BM1370 = 2,
    ASIC_BM1373 = 3,
} asic_chip_t;

/* A parsed response: a nonce (is_reg_resp 0: job_id, nonce, rolled_version,
 * asic_nr) or a register value (is_reg_resp 1: reg, data, asic_nr). */
typedef struct
{
    uint8_t job_id;
    uint32_t nonce;
    uint32_t rolled_version;
    int asic_nr;
    uint32_t data;
    uint8_t reg;
    uint8_t is_reg_resp;
} task_result;

#ifdef __cplusplus
static_assert(sizeof(task_result) == 24, "asic_driver.h layout must match rust_core");
#else
_Static_assert(sizeof(task_result) == 24, "asic_driver.h layout must match rust_core");
#endif

/* The serial port: send all bytes; rx: bytes received (up to len), 0 on a
 * timeout, < 0 on an error; clear: drop what was received. */
typedef struct {
    void *ctx;
    void (*send)(void *ctx, const uint8_t *data, size_t len);
    int (*rx)(void *ctx, uint8_t *buf, size_t len, uint16_t timeout_ms);
    void (*clear)(void *ctx);
    void (*delay_ms)(void *ctx, uint32_t ms);
} asic_uart_t;

typedef struct asic_driver asic_driver_t;

/* NULL for an unknown chip. The uart callbacks must stay valid. */
asic_driver_t *asic_driver_new(asic_chip_t chip, const asic_uart_t *uart);
void asic_driver_free(asic_driver_t *driver);

const char *asic_driver_name(const asic_driver_t *driver);
uint16_t asic_driver_small_core_count(const asic_driver_t *driver);
uint32_t asic_driver_default_vr_frequency(const asic_driver_t *driver);

/* Enumerates and configures the chain (after the board reset it), ramps to
 * frequency (MHz), sets the version rolling frequency (Hz); returns the
 * number of chips found. */
uint8_t asic_driver_init(const asic_driver_t *driver, uint64_t frequency, uint16_t asic_count, uint32_t difficulty,
                         uint32_t vr_frequency);
/* Switches the chips to the returned baud rate (the UART must follow). */
int asic_driver_set_max_baud(const asic_driver_t *driver);
/* Ramps the hash frequency in 6.25 MHz steps; false if a step has no PLL settings. */
bool asic_driver_set_frequency(const asic_driver_t *driver, float target);
/* One PLL setting without ramp; *actual (optional): the resulting frequency. */
bool asic_driver_send_hash_frequency(const asic_driver_t *driver, float target, float *actual);
/* Version rolling frequency in Hz (0 is refused). */
void asic_driver_set_vr_frequency(const asic_driver_t *driver, uint32_t freq_hz);
/* Ticket mask for a difficulty, sent only when it changes. */
void asic_driver_set_difficulty_mask(const asic_driver_t *driver, int difficulty);

/* Sends a job; returns the job ID the chips got and report back. */
uint8_t asic_driver_send_work(const asic_driver_t *driver, uint32_t job_id, uint32_t starting_nonce, uint32_t nbits,
                              uint32_t ntime, const uint8_t merkle_root_be[32], const uint8_t prev_block_hash_be[32],
                              uint32_t version);
/* Sends a complete 82-byte job (BM1368_job). */
void asic_driver_send_raw_job(const asic_driver_t *driver, const uint8_t job[82]);
/* Waits up to 60 s for a response; false on a timeout, an error or garbage. */
bool asic_driver_process_work(const asic_driver_t *driver, task_result *result);

void asic_driver_request_chip_temp(const asic_driver_t *driver);
void asic_driver_reset_counter(const asic_driver_t *driver, uint8_t reg);
void asic_driver_read_counter(const asic_driver_t *driver, uint8_t reg);

/* Low level: one frame (55 AA, header, length, data, CRC). */
void asic_driver_send_frame(const asic_driver_t *driver, uint8_t header, const uint8_t *data, size_t len);
/* Chip address spacing (normally set by init). */
void asic_driver_set_address_interval(const asic_driver_t *driver, uint8_t interval);

#ifdef __cplusplus
}
#endif

#endif /* ASIC_DRIVER_H */
