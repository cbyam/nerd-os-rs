/* C interface to the former C++ StratumApi (cpp/stratum_api.cpp, class
 * renamed to RefStratumApi) for the Rust tests, as ref_*. The same
 * interface for the current code (new_*) is in wrap_current.cpp. */
#include "stratum_api.h"
#include "transport_stub.h"

#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#define F(name) CAT(PREFIX, name)

#define API(a) (static_cast<StratumApi *>(a))
#define T(t) (static_cast<StubTransport *>(t))

extern "C" {
// Parsing without requests sent.
bool F(parse)(const char *json, StratumApiV1Message *m)
{
    static StratumApi api;
    return api.parse(m, json);
}
bool F(api_parse)(void *a, const char *json, StratumApiV1Message *m) { return API(a)->parse(m, json); }
void *F(api_new)() { return new StratumApi(); }
void F(api_free)(void *a) { delete API(a); }
char *F(receive)(void *a, void *t) { return API(a)->receiveJsonRpcLine(T(t)); }
bool F(subscribe)(void *a, void *t, const char *device, const char *asic) { return API(a)->subscribe(T(t), device, asic); }
bool F(extranonce_subscribe)(void *a, void *t) { return API(a)->entranonceSubscribe(T(t)); }
bool F(suggest_difficulty)(void *a, void *t, uint32_t d) { return API(a)->suggestDifficulty(T(t), d); }
bool F(authorize)(void *a, void *t, const char *u, const char *p) { return API(a)->authenticate(T(t), u, p); }
bool F(submit)(void *a, void *t, const char *u, const char *job, const char *en2, uint32_t ntime, uint32_t nonce,
               uint32_t version)
{
    return API(a)->submitShare(T(t), u, job, en2, ntime, nonce, version);
}
bool F(configure)(void *a, void *t) { return API(a)->configureVersionRolling(T(t)); }
}
