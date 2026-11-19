/* The former FanController and PID (c-fan/, verbatim) on a recording board
 * stub, driven by the differential test. */
#include <cstdio>
#include <cstring>
#include <string>

#include "fan_controller.h"
#include "nvs_config.h"

std::string g_fan_trace;
FanCfgStub g_fan_cfg;

void Board::getFanSpeedCh(int ch, uint16_t *r)
{
    g_fan_trace += "get " + std::to_string(ch) + "\n";
    *r = rpm[ch];
}

void Board::setFanSpeedCh(int ch, float perc)
{
    uint32_t bits;
    memcpy(&bits, &perc, 4);
    char buf[32];
    snprintf(buf, sizeof buf, "set %d %08x\n", ch, bits);
    g_fan_trace += buf;
}

namespace {
Board s_board;
FanController *s_fc;
} // namespace

extern "C" {
/* cfg: per channel mode, manual, overheat, target, p, i, d */
void reffan_setup(int fans, const uint16_t *cfg, bool use_max)
{
    s_board.numFans = fans;
    for (int ch = 0; ch < 2; ch++) {
        const uint16_t *c = cfg + 7 * ch;
        s_board.fanMode[ch] = c[0];
        g_fan_cfg.manual[ch] = c[1];
        g_fan_cfg.overheat[ch] = c[2];
        s_board.pid[ch] = {c[3], c[4], c[5], c[6]};
    }
    g_fan_cfg.use_max = use_max;
}
void reffan_init(int sample_ms)
{
    delete s_fc;
    s_fc = new FanController();
    s_fc->init(&s_board, sample_ms);
    g_fan_trace.clear();
}
void reffan_load(void) { s_fc->loadSettings(); }
void reffan_set_rpm(int ch, uint16_t rpm) { s_board.rpm[ch] = rpm; }
void reffan_update(float chip, float vr) { s_fc->update(chip, vr); }
uint16_t reffan_get(int ch, int what)
{
    switch (what) {
    case 0: return s_fc->getRPM(ch);
    case 1: return s_fc->getSpeedPerc(ch);
    case 2: return s_fc->isOverheated(ch);
    default: return s_fc->getOverheatTemp(ch);
    }
}
const char *reffan_trace(void) { return g_fan_trace.c_str(); }
void reffan_clear_trace(void) { g_fan_trace.clear(); }
}
