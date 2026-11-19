/* Scripted StratumTransport for host tests: recv() returns queued chunks,
 * then 0 (connection closed); send() records the bytes. */
#pragma once
#include <deque>
#include <string>

// the former transport class (reference copy): its io() calls the
// overridden send()/recv()
#include "../cpp/stratum_transport.h"

class StubTransport : public StratumTransport {
  public:
    StubTransport() : StratumTransport(false) {}
    std::deque<std::string> chunks;
    std::string sent;
    bool connected = true;

    int send(const void *data, size_t len) override
    {
        sent.append(static_cast<const char *>(data), len);
        return (int) len;
    }
    int recv(void *buf, size_t len) override;
    bool isConnected() override
    {
        return connected;
    }

};
