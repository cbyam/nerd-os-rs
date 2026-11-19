/* The former INA260 and DS4432U drivers (c-drivers/nerdaxe/, verbatim, C++
 * linkage) for the differential test. */
#include "DS4432U.h"
#include "INA260.h"

extern "C" {
bool refina_installed(void) { return INA260_installed(); }
float refina_current(void) { return INA260_read_current(); }
float refina_voltage(void) { return INA260_read_voltage(); }
float refina_power(void) { return INA260_read_power(); }
bool refds_test(void) { return DS4432U_test(); }
int refds_set(uint8_t output, uint8_t code) { return DS4432U_set_current_code(output, code); }
int refds_get(uint8_t output, uint8_t *code) { return DS4432U_get_current_code(output, code); }
}
