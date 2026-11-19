/* The former EMC2101 driver (c-drivers/nerdaxe/, verbatim, C++ linkage) for
 * the differential test. */
#include "EMC2101.h"

extern "C" {
bool ref2101_init(bool invert) { return EMC2101_init(invert); }
bool ref2101_polarity(bool invert) { return EMC2101_set_fan_polarity(invert); }
void ref2101_set_speed(float percent) { EMC2101_set_fan_speed(percent); }
uint16_t ref2101_get_speed(void) { return EMC2101_get_fan_speed(); }
float ref2101_external_temp(void) { return EMC2101_get_external_temp(); }
uint8_t ref2101_internal_temp(void) { return EMC2101_get_internal_temp(); }
void ref2101_ideality(uint8_t v) { EMC2101_set_ideality_factor(v); }
void ref2101_beta(uint8_t v) { EMC2101_set_beta_compensation(v); }
}
