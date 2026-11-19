/* Host stand-ins for the NVS config layer behind nvs_config.h: setters
 * append "key=value" lines to a log, getters return their default. */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "nvs_config.h"

static std::string s_log;

namespace Config {
char *cfgGetStrAlloc(const char *, const char *def) { return strdup(def); }
void cfgSetStr(const char *path, const char *value) { s_log += std::string(path) + "=s:" + value + "\n"; }
uint16_t cfgGetU16(const char *, uint16_t def) { return def; }
void cfgSetU16(const char *path, uint16_t value) { s_log += std::string(path) + "=u16:" + std::to_string(value) + "\n"; }
bool cfgGetBool(const char *, bool def) { return def; }
void cfgSetBool(const char *path, bool value) { s_log += std::string(path) + "=b:" + std::to_string(value) + "\n"; }
uint64_t cfgGetU64(const char *, uint64_t def) { return def; }
void cfgSetU64(const char *path, uint64_t value) { s_log += std::string(path) + "=u64:" + std::to_string(value) + "\n"; }
void flush() {}
} // namespace Config

extern "C" {
// the Rust settings path (src/lib.rs) logs into the same log
void cfg_log_append(const char *line) { s_log += line; }
void cfg_log_reset(void) { s_log.clear(); }
const char *cfg_log(size_t *len)
{
    *len = s_log.size();
    return s_log.data();
}
}
