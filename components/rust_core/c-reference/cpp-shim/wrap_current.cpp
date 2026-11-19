/* The C interface of wrap.cpp (new_*) for the current code: the Rust
 * stratum_v1 API that replaced StratumApi, called the way the firmware does
 * (requests formatted into a 1024 byte buffer and sent with
 * stratum_v1_send_all() by stratum_v1_run(), shares with stratum_v1_submit()). */
#include <cerrno>

#include "esp_ota_ops.h"
#include "stratum_v1.h"
#include "transport_stub.h"

namespace {
enum { BUFFER_SIZE = 1024 };

/// the stub as the socket of the Rust connection code (as the former
/// StratumTransport::io())
stratum_io_t stub_io(StubTransport *t)
{
    stratum_io_t io;
    io.ctx = t;
    io.send = [](void *ctx, const uint8_t *data, size_t len) {
        int r = static_cast<StubTransport *>(ctx)->send(data, len);
        return r >= 0 ? r : -(errno ? errno : EIO);
    };
    io.recv = [](void *ctx, uint8_t *buf, size_t len) {
        int r = static_cast<StubTransport *>(ctx)->recv(buf, len);
        return r >= 0 ? r : -(errno ? errno : EIO);
    };
    io.is_connected = [](void *ctx) { return static_cast<StubTransport *>(ctx)->isConnected(); };
    io.delay_ms = [](void *, uint32_t) {};
    return io;
}

struct Api {
    stratum_v1_session_t *session = stratum_v1_session_new();
    stratum_v1_lines_t *lines = stratum_v1_lines_new();
    char buf[BUFFER_SIZE];
    ~Api()
    {
        stratum_v1_session_free(session);
        stratum_v1_lines_free(lines);
    }
    bool send(void *t, size_t len)
    {
        if (len == 0) {
            return false;
        }
        stratum_io_t io = stub_io(static_cast<StubTransport *>(t));
        return stratum_v1_send_all(&io, (const uint8_t *) buf, len);
    }
};

Api *A(void *a) { return static_cast<Api *>(a); }
} // namespace

extern "C" {
// Parsing without requests sent: responses fall back to the fixed ID rule.
bool new_parse(const char *json, StratumApiV1Message *m)
{
    Api api;
    return stratum_v1_session_parse(api.session, json, m);
}
bool new_api_parse(void *a, const char *json, StratumApiV1Message *m)
{
    return stratum_v1_session_parse(A(a)->session, json, m);
}
void *new_api_new() { return new Api(); }
void new_api_free(void *a) { delete A(a); }
char *new_receive(void *a, void *t)
{
    stratum_io_t io = stub_io(static_cast<StubTransport *>(t));
    return stratum_v1_receive_line(&io, A(a)->lines);
}
bool new_subscribe(void *a, void *t, const char *device, const char *asic)
{
    return A(a)->send(t, stratum_v1_session_subscribe(A(a)->session, A(a)->buf, BUFFER_SIZE, device, asic,
                                                      esp_app_get_description()->version));
}
bool new_extranonce_subscribe(void *a, void *t)
{
    return A(a)->send(t, stratum_v1_session_extranonce_subscribe(A(a)->session, A(a)->buf, BUFFER_SIZE));
}
bool new_suggest_difficulty(void *a, void *t, uint32_t d)
{
    return A(a)->send(t, stratum_v1_session_suggest_difficulty(A(a)->session, A(a)->buf, BUFFER_SIZE, d));
}
bool new_authorize(void *a, void *t, const char *u, const char *p)
{
    return A(a)->send(t, stratum_v1_session_authorize(A(a)->session, A(a)->buf, BUFFER_SIZE, u, p));
}
bool new_submit(void *a, void *t, const char *u, const char *job, const char *en2, uint32_t ntime, uint32_t nonce,
                uint32_t version)
{
    stratum_io_t io = stub_io(static_cast<StubTransport *>(t));
    return stratum_v1_submit(&io, A(a)->session, u, job, en2, ntime, nonce, version);
}
bool new_configure(void *a, void *t)
{
    return A(a)->send(t, stratum_v1_session_configure(A(a)->session, A(a)->buf, BUFFER_SIZE));
}
}
