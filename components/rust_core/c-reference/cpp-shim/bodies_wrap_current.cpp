/* The current path for the bodies and URL parts of HTTP step H2b: the Rust
 * parsers (http_settings.h, http_query.h) and the CAN sink of
 * handler_can_swarm.cpp, logging like c-http/bodies_ref.cpp (the influx and
 * alert bodies with settings_apply.rs: src/lib.rs). */
#include <cstring>
#include <string>

#include "http_query.h"
#include "http_settings.h"
#include "nvs_config.h"

static void can_log(const std::string &s) { Config::cfgSetStr("can", s.c_str()); }

extern "C" {
int new_otp(const char *body, bool *enabled) { return http_otp_body(body, strlen(body), enabled); }
int new_can_node_id(const char *uri) { return http_can_node_id(uri); }
bool new_can_body(const char *body, uint8_t slave_id)
{
    settings_sink_t sink;
    sink.ctx = &slave_id;
    sink.set = [](void *ctx, uint16_t setting, int ch, uint32_t num, const char *) {
        std::string id = std::to_string(*static_cast<uint8_t *>(ctx));
        switch (setting) {
        case SETTING_CAN_FREQUENCY: can_log("freq " + id + " " + std::to_string((uint16_t) num)); break;
        case SETTING_CAN_VOLTAGE: can_log("voltage " + id + " " + std::to_string((uint16_t) num)); break;
        case SETTING_CAN_FAN:
            can_log("fan " + id + " " + std::to_string(ch) + " " + std::to_string(num & 0xff) + " " +
                    std::to_string((num >> 8) & 0xff) + " " + std::to_string((num >> 16) & 0xff) + " " +
                    std::to_string((num >> 24) & 0xff));
            break;
        case SETTING_CAN_DISPLAY:
            can_log("display " + id + " " + std::to_string(num & 1) + " " + std::to_string((num >> 1) & 1));
            break;
        default: break;
        }
    };
    return http_can_node_body(body, strlen(body), &sink);
}
int new_can_action(const char *uri)
{
    int action = -1;
    int id = http_can_node_action(uri, &action);
    if (id < 0) return -2;
    if (action < 0) return -3;
    return 100 * (action + 1) + id;
}
// The handlers pass the query only when httpd_req_get_url_query_str() fit it
// into their buffer (128 bytes for the history, 32 for allTime).
static const char *fits(const char *query, size_t buf) { return query && strlen(query) < buf ? query : nullptr; }
void new_history_query(const char *query, bool v2, history_query_t *out) { http_history_query(fits(query, 128), v2, out); }
bool new_all_time(const char *query) { return http_query_all_time(fits(query, 32)); }
}
