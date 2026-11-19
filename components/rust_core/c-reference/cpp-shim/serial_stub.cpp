/* Scripted ASIC UART for the host tests of the ASIC drivers: SERIAL_send()
 * records the bytes, SERIAL_rx() returns the queued reads, then 0 (timeout). */
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

static std::string s_sent;
static std::deque<std::string> s_reads;
static int s_clears;

static int send_impl(uint8_t *data, int len)
{
    s_sent.append((const char *) data, len);
    return len;
}
static int16_t rx_impl(uint8_t *buf, uint16_t size)
{
    if (s_reads.empty()) {
        return 0;
    }
    std::string r = s_reads.front();
    s_reads.pop_front();
    size_t n = r.size() < size ? r.size() : size;
    memcpy(buf, r.data(), n);
    return (int16_t) n;
}

// C++ linkage, like the former serial.h (c-asic/)
int SERIAL_send(uint8_t *data, int len) { return send_impl(data, len); }
int16_t SERIAL_rx(uint8_t *buf, uint16_t size, uint16_t) { return rx_impl(buf, size); }
void SERIAL_clear_buffer(void) { s_clears++; }
void SERIAL_init(void) {}
void SERIAL_set_baud(int) {}

// for the current driver's UART (asic_wrap_current.cpp)
extern "C" {
int serial_stub_send(uint8_t *data, int len) { return send_impl(data, len); }
int16_t serial_stub_rx(uint8_t *buf, uint16_t size) { return rx_impl(buf, size); }
void serial_stub_clear(void) { s_clears++; }
}

extern "C" {
void serial_reset(void)
{
    s_sent.clear();
    s_reads.clear();
    s_clears = 0;
}
void serial_push_read(const uint8_t *data, size_t len) { s_reads.emplace_back((const char *) data, len); }
const uint8_t *serial_sent(size_t *len)
{
    *len = s_sent.size();
    return (const uint8_t *) s_sent.data();
}
int serial_clears(void) { return s_clears; }
}
