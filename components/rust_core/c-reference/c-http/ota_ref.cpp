/* The former input handling of the factory update, verbatim from
 * main/http_server/handler_ota_factory.cpp (before HTTP step H3):
 * is_safe_github_url() and the reading of the JSON body. */
#include <cstring>
#include <strings.h>

#include "ArduinoJson.h"

#define GITHUB_REPO "https://github.com/shufps/"

// --- Helper: validate URL is a safe GitHub link to shufps repo
static bool is_safe_github_url(const char *url)
{
    if (!url)
        return false;

    // check url stats with allowed github repo url
    if (strncasecmp(url, GITHUB_REPO, strlen(GITHUB_REPO)) != 0)
        return false;

    // Reject traversal and encoded traversal
    if (strstr(url, "../") || strstr(url, "/..") || strstr(url, "%2e%2e") || strstr(url, "%2E%2E"))
        return false;

    // Reject backslashes (avoid weird normalization)
    if (strchr(url, '\\'))
        return false;

    // Reject control chars
    for (const char *q = url; *q; ++q) {
        unsigned char c = (unsigned char) *q;
        if (c < 0x20 || c == 0x7F)
            return false;
    }

    return true;
}


extern "C" {
bool ref_factory_url_ok(const char *url) { return is_safe_github_url(url); }

// 0 ok, 1 invalid JSON, 2 url missing
int ref_factory_body(const char *buf, const char **url_out, bool *keep_out)
{
    static JsonDocument doc;
    doc.clear();
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        return 1;
    }
    const char *url = doc["url"] | (const char *) nullptr;
    if (!url) {
        return 2;
    }

    // keep_config defaults to true (safe default: don't wipe config unless explicitly requested)
    bool keep_config = doc["keep_config"] | true;

    *url_out = url;
    *keep_out = keep_config;
    return 0;
}
}
