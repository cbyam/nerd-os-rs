/* The former EMC2302 driver (c-drivers/, verbatim, C++ linkage) for the
 * differential test. */
#include "EMC2302.h"

extern "C" {
bool ref_emc_init(bool invert) { return EMC2302_init(invert); }
bool ref_emc_polarity(bool invert) { return EMC2302_set_fan_polarity(invert); }
int ref_emc_set_speed(int channel, float percent) { return EMC2302_set_fan_speed(channel, percent); }
int ref_emc_get_speed(int channel, uint16_t *dst) { return EMC2302_get_fan_speed(channel, dst); }
}
