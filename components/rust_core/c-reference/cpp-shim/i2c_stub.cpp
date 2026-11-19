/* The I2C command-link API on the host: a link's operations are turned into
 * one SMBus operation ("rb 71 fc", "ww 71 21 0096", ...) appended to a trace;
 * reads are served from a fake register file. */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "driver/i2c.h"
#include "i2c_master.h"

namespace {
struct Op {
    enum { START, STOP, WRITE, READ } kind;
    uint8_t byte;
    uint8_t *dst;
    size_t len;
};
struct Link {
    std::vector<Op> ops;
};
uint16_t s_regs[256];
std::string s_trace;
bool s_fail;
} // namespace

extern "C" {
void i2c_stub_reset(void)
{
    memset(s_regs, 0, sizeof(s_regs));
    s_trace.clear();
    s_fail = false;
}
void i2c_stub_set(uint8_t reg, uint16_t value) { s_regs[reg] = value; }
const char *i2c_stub_trace(void) { return s_trace.c_str(); }
void i2c_stub_clear_trace(void) { s_trace.clear(); }

i2c_cmd_handle_t i2c_cmd_link_create(void) { return new Link(); }
void i2c_cmd_link_delete(i2c_cmd_handle_t cmd) { delete static_cast<Link *>(cmd); }
esp_err_t i2c_master_start(i2c_cmd_handle_t cmd)
{
    static_cast<Link *>(cmd)->ops.push_back({Op::START, 0, nullptr, 0});
    return ESP_OK;
}
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd)
{
    static_cast<Link *>(cmd)->ops.push_back({Op::STOP, 0, nullptr, 0});
    return ESP_OK;
}
esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool)
{
    static_cast<Link *>(cmd)->ops.push_back({Op::WRITE, data, nullptr, 0});
    return ESP_OK;
}
esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd, uint8_t *data, i2c_ack_type_t)
{
    static_cast<Link *>(cmd)->ops.push_back({Op::READ, 0, data, 1});
    return ESP_OK;
}
esp_err_t i2c_master_read(i2c_cmd_handle_t cmd, uint8_t *data, size_t len, i2c_ack_type_t)
{
    static_cast<Link *>(cmd)->ops.push_back({Op::READ, 0, data, len});
    return ESP_OK;
}

/* register-level API (i2c_master.h): byte registers from the low bytes of
 * the register file */
esp_err_t i2c_master_register_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    char buf[48];
    snprintf(buf, sizeof buf, "rr %02x %02x %u\n", addr, reg, (unsigned) len);
    s_trace += buf;
    if (s_fail) return ESP_FAIL;
    for (size_t i = 0; i < len; i++) data[i] = (uint8_t) s_regs[(uint8_t) (reg + i)];
    return ESP_OK;
}
esp_err_t i2c_master_register_write_byte(uint8_t addr, uint8_t reg, uint8_t data)
{
    char buf[48];
    snprintf(buf, sizeof buf, "wr %02x %02x %02x\n", addr, reg, data);
    s_trace += buf;
    return s_fail ? ESP_FAIL : ESP_OK;
}
esp_err_t i2c_master_register_write_word(uint8_t addr, uint8_t reg, uint16_t data)
{
    char buf[48];
    snprintf(buf, sizeof buf, "wrw %02x %02x %04x\n", addr, reg, data);
    s_trace += buf;
    return s_fail ? ESP_FAIL : ESP_OK;
}
void i2c_stub_fail(bool fail) { s_fail = fail; }

/* FreeRTOS ticks of the host build (c-drivers-shim/freertos) */
static uint32_t s_ticks;
uint32_t xTaskGetTickCount(void) { return s_ticks; }
void vTaskDelay(uint32_t ticks)
{
    char buf[32];
    snprintf(buf, sizeof buf, "delay %u\n", (unsigned) ticks);
    s_trace += buf;
    s_ticks += ticks;
}

/* the link as an SMBus operation */
esp_err_t i2c_master_cmd_begin(i2c_port_t, i2c_cmd_handle_t cmd, TickType_t)
{
    Link *l = static_cast<Link *>(cmd);
    std::vector<uint8_t> written;
    size_t reads = 0;
    std::vector<std::pair<uint8_t *, size_t>> dsts;
    for (auto &op : l->ops) {
        if (op.kind == Op::WRITE) written.push_back(op.byte);
        if (op.kind == Op::READ) {
            reads += op.len;
            dsts.push_back({op.dst, op.len});
        }
    }
    char buf[64];
    if (written.empty()) return ESP_FAIL;
    uint8_t addr = written[0] >> 1;
    if (reads) {
        // write addr, cmd, then read addr
        uint8_t reg = written[1];
        uint16_t v = s_regs[reg];
        uint8_t bytes[2] = {(uint8_t) (v & 0xff), (uint8_t) (v >> 8)};
        size_t i = 0;
        for (auto &d : dsts)
            for (size_t k = 0; k < d.second; k++) d.first[k] = bytes[i++ & 1];
        snprintf(buf, sizeof buf, "%s %02x %02x\n", reads == 1 ? "rb" : "rw", addr, reg);
    } else if (written.size() == 2) {
        snprintf(buf, sizeof buf, "wc %02x %02x\n", addr, written[1]);
    } else if (written.size() == 3) {
        snprintf(buf, sizeof buf, "wb %02x %02x %02x\n", addr, written[1], written[2]);
    } else if (written.size() == 4) {
        snprintf(buf, sizeof buf, "ww %02x %02x %04x\n", addr, written[1], written[2] | (written[3] << 8));
    } else {
        // block write: command, length byte, data
        std::string line = "wblk";
        for (size_t i = 0; i < written.size(); i++) {
            snprintf(buf, sizeof buf, " %02x", i == 0 ? addr : written[i]);
            line += buf;
        }
        s_trace += line + "\n";
        return s_fail ? ESP_FAIL : ESP_OK;
    }
    s_trace += buf;
    return s_fail ? ESP_FAIL : ESP_OK;
}
}
