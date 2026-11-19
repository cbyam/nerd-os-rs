/* The former NerdAxe TPS546 driver (c-drivers/nerdaxe/, verbatim), driven
 * by the differential test through these C functions. */
#include <cstdint>

#include "TPS546.h"

extern "C" {
void ref546_set_vin_config(float on, float off, float uv, float ov) { TPS546_set_vin_config(on, off, uv, ov); }
void ref546_set_iout_config(float warn, float fault) { TPS546_set_iout_config(warn, fault); }
int ref546_init(void) { return TPS546_init(); }
int ref546_get_frequency(void) { return TPS546_get_frequency(); }
void ref546_set_frequency(int khz) { TPS546_set_frequency(khz); }
float ref546_read(int what)
{
    switch (what) {
    case 0: return TPS546_get_temperature();
    case 1: return TPS546_get_vin();
    case 2: return TPS546_get_iout();
    default: return TPS546_get_vout();
    }
}
bool ref546_set_vout(float v) { return TPS546_set_vout(v); }
void ref546_print_status(void) { TPS546_print_status(); }
}
