/* The former TPS53647/TPS53667 drivers (c-drivers/, verbatim), driven by the
 * differential test; compiled once without and once with -DQ1373 (phase
 * shedding), the classes and these functions renamed per build. */
#include <cstdint>

#include "TPS53647.h"
#include "TPS53667.h"

namespace {
TPS53647 *s_dev;
}

extern "C" {
/* chip 0: TPS53647, 1: TPS53667 */
bool REFNAME(init)(int chip, int phases, int imax, float ifault)
{
    delete s_dev;
    s_dev = chip ? new TPS53667() : new TPS53647();
    return s_dev->init(phases, imax, ifault);
}
bool REFNAME(set_vout)(float v) { return s_dev->set_vout(v); }
float REFNAME(read)(int what)
{
    switch (what) {
    case 0: return s_dev->get_temperature();
    case 1: return s_dev->get_pin();
    case 2: return s_dev->get_pout();
    case 3: return s_dev->get_vin();
    case 4: return s_dev->get_iin();
    case 5: return s_dev->get_iout();
    default: return s_dev->get_vout();
    }
}
uint16_t REFNAME(vout_vid)(void) { return s_dev->get_vout_vid(); }
uint8_t REFNAME(status_reg)(int which)
{
    switch (which) {
    case 0: return s_dev->get_status_byte();
    case 1: return s_dev->get_status_iout();
    case 2: return s_dev->get_status_vout();
    case 3: return s_dev->get_status_input();
    default: return s_dev->get_status_temp();
    }
}
void REFNAME(status)(void) { s_dev->status(); }
void REFNAME(clear_faults)(void) { s_dev->clear_faults(); }
uint16_t REFNAME(device_code)(void) { return s_dev->get_device_code(); }
}
