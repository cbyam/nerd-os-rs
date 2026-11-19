/*
 * Host stand-ins for the ESP-IDF functions rust_core calls (tests only).
 * The tests install hooks to script the transport and the random source;
 * without hooks, transport calls fail and randomness comes from a fixed
 * xorshift sequence.
 */
#include <stddef.h>
#include <stdint.h>

typedef int (*esp_host_rw_hook)(void *transport, char *buf, int len, int timeout_ms);
typedef void (*esp_host_random_hook)(void *buf, size_t len);

static esp_host_rw_hook s_read;
static esp_host_rw_hook s_write;
static esp_host_random_hook s_random;

void esp_host_set_hooks(esp_host_rw_hook read, esp_host_rw_hook write, esp_host_random_hook random)
{
    s_read = read;
    s_write = write;
    s_random = random;
}

int esp_transport_read(void *t, char *buf, int len, int timeout_ms)
{
    return s_read ? s_read(t, buf, len, timeout_ms) : -1;
}

int esp_transport_poll_read(void *t, int timeout_ms)
{
    (void) t;
    (void) timeout_ms;
    return 1; /* readable: the scripted reads decide */
}

int esp_transport_write(void *t, const char *buf, int len, int timeout_ms)
{
    return s_write ? s_write(t, (char *) buf, len, timeout_ms) : -1;
}

void esp_fill_random(void *buf, size_t len)
{
    if (s_random) {
        s_random(buf, len);
        return;
    }
    static uint64_t x = 0x9e3779b97f4a7c15ull;
    for (size_t i = 0; i < len; i++) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        ((uint8_t *) buf)[i] = (uint8_t) ((x * 0x2545f4914f6cdd1dull) >> 56);
    }
}
