#include <cstring>

#include "esp_ota_ops.h"
#include "transport_stub.h"

// The real implementation (stratum_transport.cpp) needs esp_transport.
StratumTransport::StratumTransport(bool use_tls) : m_use_tls(use_tls), m_t(nullptr) {}
StratumTransport::~StratumTransport() {}
bool StratumTransport::connect(const char *, const char *, uint16_t) { return false; }
int StratumTransport::send(const void *, size_t) { return -1; }
int StratumTransport::recv(void *, size_t) { return -1; }
bool StratumTransport::isConnected() { return false; }
void StratumTransport::close() {}

int StubTransport::recv(void *buf, size_t len)
{
    if (chunks.empty()) {
        return 0;
    }
    std::string &c = chunks.front();
    size_t n = c.size() < len ? c.size() : len;
    memcpy(buf, c.data(), n);
    c.erase(0, n);
    if (c.empty()) {
        chunks.pop_front();
    }
    return (int) n;
}

const esp_app_desc_t *esp_app_get_description(void)
{
    static const esp_app_desc_t desc = {"v-test"};
    return &desc;
}

/* strtoul() as newlib implements it with a 32-bit unsigned long (ESP32),
 * used by the reference build of stratum_api.cpp (-Dstrtoul=...). */
extern "C" unsigned long strtoul_newlib32(const char *s, char **end, int base)
{
    (void) end;
    (void) base; // always 16 in stratum_api.cpp
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    bool neg = false;
    if (*s == '-') { neg = true; s++; } else if (*s == '+') { s++; }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t acc = 0;
    bool overflow = false;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        if (acc > (0xFFFFFFFFu - d) / 16) overflow = true;
        else if (!overflow) acc = acc * 16 + d;
    }
    if (overflow) return 0xFFFFFFFFu;
    return neg ? (uint32_t) (0u - acc) : acc;
}

extern "C" {
void *stub_new() { return new StubTransport(); }
void stub_free(void *t) { delete static_cast<StubTransport *>(t); }
void stub_push(void *t, const char *data, size_t len) { static_cast<StubTransport *>(t)->chunks.emplace_back(data, len); }
const char *stub_sent(void *t, size_t *len)
{
    auto *s = static_cast<StubTransport *>(t);
    *len = s->sent.size();
    return s->sent.data();
}
void stub_clear_sent(void *t) { static_cast<StubTransport *>(t)->sent.clear(); }
}
