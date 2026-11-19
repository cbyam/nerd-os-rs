/*
 * Board-independent logic of the Board classes (implemented in Rust,
 * components/rust_core/src/board_core.rs): frequency/voltage limits, the
 * chip temperatures, the TPS536x7 PMBus fault decoding.
 */

#ifndef BOARD_CORE_H
#define BOARD_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the absolute limits of a board (0: no limit) */
typedef struct {
    int abs_max_frequency;
    int abs_min_voltage_mv;
    int abs_max_voltage_mv;
} board_limits_t;

/* 0 V (output off) always passes */
bool board_validate_voltage(float volts, const board_limits_t *l);
bool board_validate_frequency(float mhz, const board_limits_t *l);
/* the stored frequency/voltage within the limits */
void board_clamp_settings(uint16_t frequency, uint16_t voltage_mv, const board_limits_t *l, int *out_frequency,
                          int *out_voltage_mv);
/* the Board::Error of the TPS536x7 status registers (0xff: unreadable), the
 * status word into *status */
int board_pmbus_fault(uint8_t byte, uint8_t iout, uint8_t vout, uint8_t input, uint8_t temp, uint32_t *status);

/* the ASIC chip temperatures (0 for an unknown chip) */
void board_chip_temps_init(int count);
void board_chip_temp_set(int nr, float temp);
float board_chip_temp(int nr);
float board_chip_temp_max(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CORE_H */
