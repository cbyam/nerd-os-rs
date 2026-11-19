/* The former FXL6408 and TMP451 drivers (c-drivers/, verbatim) for the
 * differential test. */
#include <cmath>

#include "fxl6408.h"
#include "tmp451.h"

namespace {
Fxl6408 *s_fxl;
Tmp451 *s_tmp;
} // namespace

extern "C" {
void ref_fxl_new(void)
{
    delete s_fxl;
    s_fxl = new Fxl6408();
}
bool ref_fxl_init(void) { return s_fxl->init(); }
int ref_fxl_set_direction(uint8_t pin, bool output) { return s_fxl->set_direction(pin, output); }
int ref_fxl_write(uint8_t pin, bool level) { return s_fxl->write(pin, level); }
int ref_fxl_pull_up(uint8_t pin) { return s_fxl->enable_pull_up(pin); }
int ref_fxl_input(uint8_t *v) { return s_fxl->inputStatus(v); }
int ref_fxl_read_pin(uint8_t pin, bool *level) { return s_fxl->read_pin(pin, level); }

void ref_tmp_new(uint8_t addr)
{
    delete s_tmp;
    s_tmp = new Tmp451(addr);
}
int ref_tmp_init(void) { return s_tmp->init(); }
float ref_tmp_temperature(int ch) { return s_tmp->get_temperature(ch); }
float ref_tmp_raw(int ch) { return s_tmp->get_raw_temperature(ch); }
void ref_tmp_cal(float s, float a, float b, float c, float d) { s_tmp->set_temp_cal(s, a, b, c, d); }
}
