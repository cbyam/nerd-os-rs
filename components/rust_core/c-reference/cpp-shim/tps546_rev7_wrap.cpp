/* The former rev7 TPS546 driver (c-drivers/rev7/, verbatim, namespace
 * Rev7TPS546), driven by the differential test: the free functions with the
 * single-converter default configuration and the BuckConverter class (dual
 * configuration, voltage domains). */
#include <cstdint>

#include "TPS546.h"

using namespace Rev7TPS546;

namespace {
TPS546 *s_dev;
}

extern "C" {
bool ref7_probe(void) { return TPS546_probe(); }
int ref7_init_single(void) { return TPS546_init(TPS546_create_default_config()); }
bool ref7_set_vout_single(float v) { return TPS546_set_vout(v); }

bool ref7_new(uint16_t domains)
{
    delete s_dev;
    s_dev = new TPS546(domains);
    return s_dev->init(0, 0, 0);
}
float ref7_read(int what)
{
    switch (what) {
    case 0: return s_dev->get_temperature();
    case 1: return s_dev->get_vin();
    case 2: return s_dev->get_iout();
    case 3: return s_dev->get_vout();
    case 4: return s_dev->get_pout();
    default: return s_dev->get_iin();
    }
}
bool ref7_set_vout(float v) { return s_dev->set_vout(v); }
bool ref7_disable(void) { return s_dev->disable_vout(); }
void ref7_clear_faults(void) { s_dev->clear_faults(); }
void ref7_status(void) { s_dev->status(); }
uint8_t ref7_status_reg(int which)
{
    switch (which) {
    case 0: return s_dev->get_status_byte();
    case 1: return s_dev->get_status_iout();
    case 2: return s_dev->get_status_vout();
    case 3: return s_dev->get_status_input();
    default: return s_dev->get_status_temp();
    }
}
}
