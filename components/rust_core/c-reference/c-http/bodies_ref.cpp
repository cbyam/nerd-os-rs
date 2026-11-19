/* The former handling of the remaining request bodies and URL parts,
 * verbatim from main/http_server/ (before HTTP step H2b): handler_influx.cpp,
 * handler_alert.cpp, handler_otp.cpp (PATCH), handler_can_swarm.cpp,
 * handler_system.cpp (history query, allTime) and v2/handler_v2_dashboard.cpp
 * (history query); plus httpd_query_key_value() from ESP-IDF
 * (components/esp_http_server/src/httpd_parse.c). The request is replaced by
 * the URI or query string; config writes go to cfg_stub.cpp, CAN commands
 * to the same log. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <string>

#include "ArduinoJson.h"
#include "nvs_config.h"

#define CAN_SLAVE_MAX 32
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_HTTPD_RESULT_TRUNC 0xb008
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ESP_LOGD(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
#define ESP_LOGE(...) ((void) 0)
typedef int esp_err_t;

/* glibc has strlcpy only from 2.38 (the CI image has 2.35) */
static size_t ref_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
#define strlcpy ref_strlcpy

/* newlib on the ESP32: long is 32 bits */
static long strtol_newlib32(const char *s, char **, int)
{
    long long v = strtoll(s, nullptr, 10);
    return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (long) v;
}
#define atoi(s) ((int) strtol_newlib32((s), nullptr, 10))
static unsigned long strtoul_newlib32_10(const char *s, char **, int)
{
    // strtoull() modulo 2^64 with the 32-bit range: overflow is ULONG_MAX
    const char *p = s;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    bool neg = *p == '-';
    if (*p == '-' || *p == '+') p++;
    uint64_t acc = 0;
    bool overflow = false;
    for (; *p >= '0' && *p <= '9'; p++) {
        acc = acc * 10 + (*p - '0');
        if (acc > 0xFFFFFFFFull) overflow = true;
        if (overflow) acc = 0xFFFFFFFFull;
    }
    if (overflow) return 0xFFFFFFFFul;
    return neg ? (uint32_t) (0u - (uint32_t) acc) : (uint32_t) acc;
}
#define strtoul strtoul_newlib32_10

/* ESP-IDF httpd_query_key_value(), verbatim */
static esp_err_t httpd_query_key_value(const char *qry_str, const char *key, char *val, size_t val_size)
{
    if (qry_str == NULL || key == NULL || val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char   *qry_ptr = qry_str;
    const size_t  buf_len = val_size;

    while (strlen(qry_ptr)) {
        const char *val_ptr = strchr(qry_ptr, '=');
        if (!val_ptr) {
            break;
        }
        size_t offset = val_ptr - qry_ptr;

        if ((offset != strlen(key)) ||
            (strncasecmp(qry_ptr, key, offset))) {
            qry_ptr = strchr(val_ptr, '&');
            if (!qry_ptr) {
                break;
            }
            qry_ptr++;
            continue;
        }

        qry_ptr = strchr(++val_ptr, '&');
        if (!qry_ptr) {
            qry_ptr = val_ptr + strlen(val_ptr);
        }

        val_size = qry_ptr - val_ptr + 1;

        strlcpy(val, val_ptr, MIN(val_size, buf_len));

        if (buf_len < val_size) {
            return ESP_ERR_HTTPD_RESULT_TRUNC;
        }
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* the request: its URI and query (NULL: none) */
struct Req {
    const char *uri;
    const char *query;
};
static esp_err_t httpd_req_get_url_query_str(Req *req, char *buf, size_t buf_len)
{
    if (!req->query) return ESP_ERR_NOT_FOUND;
    size_t min_buf_len = strlen(req->query) + 1;
    strlcpy(buf, req->query, MIN(buf_len, min_buf_len));
    return buf_len < min_buf_len ? ESP_ERR_HTTPD_RESULT_TRUNC : ESP_OK;
}

/* responses become codes */
enum { RESP_MISSING_ID = 1, RESP_INVALID_ID = 2, RESP_UNKNOWN_ACTION = 3, RESP_INVALID_URI = 4 };
#define httpd_resp_send_err(req, code, msg) (strcmp(msg, "missing id") == 0 ? RESP_MISSING_ID : strcmp(msg, "invalid id") == 0 ? RESP_INVALID_ID : strcmp(msg, "unknown action") == 0 ? RESP_UNKNOWN_ACTION : RESP_INVALID_URI)
#define HTTPD_400_BAD_REQUEST 400

/* CAN commands into the config log */
namespace Config { void cfgSetStr(const char *path, const char *value); }
static void can_log(const std::string &s) { Config::cfgSetStr("can", s.c_str()); }
static void can_master_set_slave_freq(uint8_t id, uint16_t f) { can_log("freq " + std::to_string(id) + " " + std::to_string(f)); }
static void can_master_set_slave_voltage(uint8_t id, uint16_t v) { can_log("voltage " + std::to_string(id) + " " + std::to_string(v)); }
static void can_master_set_slave_fan(uint8_t id, uint8_t ch, uint8_t mode, uint8_t speed, uint8_t target, uint8_t overheat)
{
    can_log("fan " + std::to_string(id) + " " + std::to_string(ch) + " " + std::to_string(mode) + " " + std::to_string(speed) + " " +
            std::to_string(target) + " " + std::to_string(overheat));
}
static void can_master_set_slave_display(uint8_t id, uint8_t flip, uint8_t off)
{
    can_log("display " + std::to_string(id) + " " + std::to_string(flip) + " " + std::to_string(off));
}
static int s_action;
static void can_master_restart_slave(uint8_t id) { s_action = 100 + id; }
static void can_master_shutdown_slave(uint8_t id) { s_action = 200 + id; }
static void can_master_identify_slave(uint8_t id) { s_action = 300 + id; }

static bool parse_doc(JsonDocument &doc, const char *buf)
{
    DeserializationError error = deserializeJson(doc, buf);
    return !error;
}

static esp_err_t parse_slave_id(Req *req, uint8_t *out_id)
{
    const char *uri = req->uri;
    // URI format: /api/v2/can/nodes/{id}/action — find second-to-last slash
    const char *last = strrchr(uri, '/');
    if (!last || last == uri) return ESP_FAIL;
    const char *prev = last - 1;
    while (prev > uri && *prev != '/') prev--;
    if (*prev != '/') return ESP_FAIL;
    int id = atoi(prev + 1);
    if (id < 1 || id >= CAN_SLAVE_MAX) return ESP_FAIL;
    *out_id = (uint8_t) id;
    return ESP_OK;
}


extern "C" {
bool ref_influx(const char *buf)
{
    JsonDocument doc;
    if (!parse_doc(doc, buf)) return false;
    // Check and apply each setting if the key exists and has the correct type
    if (doc["enabled"].is<bool>()) {
        Config::setInfluxEnabled(doc["enabled"].as<bool>());
    }
    if (doc["url"].is<const char*>()) {
        Config::setInfluxURL(doc["url"].as<const char*>());
    }
    if (doc["port"].is<uint16_t>()) {
        Config::setInfluxPort(doc["port"].as<uint16_t>());
    }
    if (doc["token"].is<const char*>()) {
        Config::setInfluxToken(doc["token"].as<const char*>());
    }
    if (doc["bucket"].is<const char*>()) {
        Config::setInfluxBucket(doc["bucket"].as<const char*>());
    }
    if (doc["org"].is<const char*>()) {
        Config::setInfluxOrg(doc["org"].as<const char*>());
    }
    if (doc["prefix"].is<const char*>()) {
        Config::setInfluxPrefix(doc["prefix"].as<const char*>());
    }

    return true;
}

bool ref_alert(const char *buf)
{
    JsonDocument doc;
    if (!parse_doc(doc, buf)) return false;
    if (doc["webhookUrl"].is<const char*>()) {
        Config::setDiscordWebhook(doc["webhookUrl"].as<const char*>());
    }
    if (doc["watchdogEnable"].is<bool>()) {
        Config::setDiscordWatchdogAlertEnabled(doc["watchdogEnable"].as<bool>());
    }
    if (doc["blockFoundEnable"].is<bool>()) {
        Config::setDiscordAlertBlockFoundEnabled(doc["blockFoundEnable"].as<bool>());
    }
    if (doc["bestDiffEnable"].is<bool>()) {
        Config::setDiscordAlertBestDiffEnabled(doc["bestDiffEnable"].as<bool>());
    }
    if (doc["coinbaseVerifyEnable"].is<bool>()) {
        Config::setDiscordCoinbaseVerifyAlertEnabled(doc["coinbaseVerifyEnable"].as<bool>());
    }
    if (doc["showBlockFoundScreen"].is<bool>()) {
        Config::setShowBlockFoundEnabled(doc["showBlockFoundScreen"].as<bool>());
    }

    return true;
}

// 0 ok, 1 invalid JSON, 2 invalid enabled state
int ref_otp(const char *buf, bool *out)
{
    JsonDocument doc;
    if (!parse_doc(doc, buf)) return 1;
    if (!doc["enabled"].is<bool>()) {
        ESP_LOGE(TAG, "invalid enabled state");
        return 2;
    }

    bool enabled = doc["enabled"].as<bool>();

    *out = doc["enabled"].as<bool>();
    return 0;
}

// the id of PATCH/DELETE /api/v2/can/nodes/{id}, or -RESP_*
int ref_can_node_id(const char *uri_in)
{
    Req r{uri_in, nullptr};
    Req *req = &r;
    // Extract id from URI: /api/v2/can/nodes/{id}
    const char *uri = req->uri;
    const char *last_slash = strrchr(uri, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return -httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
    }
    int id = atoi(last_slash + 1);
    if (id < 1 || id >= CAN_SLAVE_MAX) {
        return -httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid id");
    }
    uint8_t slave_id = (uint8_t) id;

    return slave_id;
}

bool ref_can_body(const char *buf, uint8_t slave_id)
{
    JsonDocument doc;
    if (!parse_doc(doc, buf)) return false;
    // field names matching info endpoint PATCH
    if (doc["frequency"].is<uint16_t>())
        can_master_set_slave_freq(slave_id, doc["frequency"].as<uint16_t>());

    if (doc["coreVoltage"].is<uint16_t>())
        can_master_set_slave_voltage(slave_id, doc["coreVoltage"].as<uint16_t>());

    // fans array — matches info endpoint PATCH structure
    if (doc["fans"].is<JsonArray>()) {
        JsonArray fans = doc["fans"].as<JsonArray>();
        int ch = 0;
        for (JsonObject fan : fans) {
            if (ch > 1) break;
            uint8_t mode       = fan["mode"].as<uint8_t>();
            uint8_t speed      = fan["manualSpeed"].as<uint8_t>();
            uint8_t targetTemp = fan["targetTemp"].as<uint8_t>();
            uint8_t overheat   = fan["overheatTemp"].as<uint8_t>();
            can_master_set_slave_fan(slave_id, (uint8_t) ch, mode, speed, targetTemp, overheat);
            ch++;
        }
    }

    if (doc["flipScreen"].is<bool>() || doc["autoScreenOff"].is<bool>()) {
        uint8_t flip    = doc["flipScreen"].as<bool>()    ? 1 : 0;
        uint8_t autoOff = doc["autoScreenOff"].as<bool>() ? 1 : 0;
        can_master_set_slave_display(slave_id, flip, autoOff);
    }

    return true;
}

// POST /api/v2/can/nodes/{id}/{action}: 100/200/300 + id, or -RESP_*
int ref_can_action(const char *uri)
{
    Req r{uri, nullptr};
    Req *req = &r;
    uint8_t slave_id;
    if (parse_slave_id(req, &slave_id) != ESP_OK)
        return -httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid id");
    s_action = 0;
    // Dispatch based on the last path segment (restart, shutdown, identify)
    const char *last = strrchr(req->uri, '/');
    if (!last) return -httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid uri");
    last++; // skip the '/'

    if (strcmp(last, "restart") == 0) {
        can_master_restart_slave(slave_id);
    } else if (strcmp(last, "shutdown") == 0) {
        can_master_shutdown_slave(slave_id);
    } else if (strcmp(last, "identify") == 0) {
        can_master_identify_slave(slave_id);
    } else {
        return -httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown action");
    }

    return s_action;
}

struct HistoryQuery {
    uint64_t start, current, span_ms;
    uint32_t limit;
    bool requested;
};

void ref_history_query(const char *query, bool v2, HistoryQuery *out)
{
    Req r{"/", query};
    Req *req = &r;
    const uint64_t DEFAULT_HISTORY_SPAN_MS = 3600ULL * 1000ULL;
    const uint64_t MAX_HISTORY_SPAN_MS = 3ULL * 3600ULL * 1000ULL;
    uint64_t start_timestamp = 0;
    uint64_t current_timestamp = 0;
    uint32_t history_limit = 0;
    bool history_requested = false;
    uint64_t history_span_ms = DEFAULT_HISTORY_SPAN_MS;
    char query_str[128];
    if (v2) {
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
            char param[64];
            if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
                start_timestamp = strtoull(param, NULL, 10);
                if (start_timestamp) history_requested = true;
            }
            if (httpd_query_key_value(query_str, "limit", param, sizeof(param)) == ESP_OK) {
                history_limit = strtoul(param, NULL, 10);
                if (history_limit > 1000) history_limit = 1000;
            }
            if (httpd_query_key_value(query_str, "historySpan", param, sizeof(param)) == ESP_OK) {
                history_span_ms = strtoull(param, NULL, 10);
                if (history_span_ms > MAX_HISTORY_SPAN_MS) history_span_ms = MAX_HISTORY_SPAN_MS;
                if (history_span_ms == 0) history_span_ms = DEFAULT_HISTORY_SPAN_MS;
            }
            if (httpd_query_key_value(query_str, "cur", param, sizeof(param)) == ESP_OK) {
                current_timestamp = strtoull(param, NULL, 10);
            }
        }
    
    
    } else {
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
            char param[64];
            if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
                start_timestamp = strtoull(param, NULL, 10);
                if (start_timestamp) {
                    history_requested = true;
                }
            }
            if (httpd_query_key_value(query_str, "limit", param, sizeof(param)) == ESP_OK) {
                history_limit = strtoul(param, NULL, 10);
                if (history_limit > 1000) {
                    history_limit = 1000;
                }
            }
            if (httpd_query_key_value(query_str, "history_span", param, sizeof(param)) == ESP_OK) {
                history_span_ms = strtoull(param, NULL, 10);
                if (history_span_ms > MAX_HISTORY_SPAN_MS) {
                    history_span_ms = MAX_HISTORY_SPAN_MS;
                }
                if (history_span_ms == 0) {
                    history_span_ms = DEFAULT_HISTORY_SPAN_MS;
                }
            }
            if (httpd_query_key_value(query_str, "cur", param, sizeof(param)) == ESP_OK) {
                current_timestamp = strtoull(param, NULL, 10);
                ESP_LOGI(TAG, "cur: %llu", current_timestamp);
            }
        }
    
    
    }
    *out = HistoryQuery{start_timestamp, current_timestamp, history_span_ms, history_limit, history_requested};
}

bool ref_all_time(const char *query_in)
{
    Req r{"/", query_in};
    Req *req = &r;
    bool allTime = false;
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char value[8];
        if (httpd_query_key_value(query, "allTime", value, sizeof(value)) == ESP_OK) {
            allTime = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
    }

    return allTime;
}
}
