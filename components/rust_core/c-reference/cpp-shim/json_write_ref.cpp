/* ArduinoJson's serializer (the reference for ajson::Json::serialize). */
#include <cstddef>
#include <cstdint>

#include "ArduinoJson.h"

extern "C" {
/* a float (is_float) or a double as a document, serialized */
size_t ref_json_number(double v, bool is_float, char *out, size_t cap)
{
    JsonDocument doc;
    if (is_float) {
        doc.set((float) v);
    } else {
        doc.set(v);
    }
    return serializeJson(doc, out, cap);
}

/* deserialize + serialize; SIZE_MAX if the input is invalid */
size_t ref_json_roundtrip(const char *in, size_t len, char *out, size_t cap)
{
    JsonDocument doc;
    if (deserializeJson(doc, in, len)) {
        return SIZE_MAX;
    }
    return serializeJson(doc, out, cap);
}
}

/* The accessors of the former main/nvs_config.cpp on a document. */
static JsonDocument s_cfg;

extern "C" {
bool ref_cfg_load(const char *json)
{
    s_cfg.clear();
    return !deserializeJson(s_cfg, json);
}
uint16_t ref_cfg_get_u16(const char *key, uint16_t def)
{
    JsonVariant v = s_cfg[key];
    return v.isNull() ? def : v.as<uint16_t>();
}
uint64_t ref_cfg_get_u64(const char *key, uint64_t def)
{
    JsonVariant v = s_cfg[key];
    return v.isNull() ? def : v.as<uint64_t>();
}
bool ref_cfg_get_bool(const char *key, bool def)
{
    JsonVariant v = s_cfg[key];
    if (v.isNull()) return def;
    if (v.is<bool>()) return v.as<bool>();
    return v.as<uint16_t>() != 0;
}
/* the string or NULL (is<const char*>) */
const char *ref_cfg_get_str(const char *key)
{
    JsonVariant v = s_cfg[key];
    return v.is<const char *>() ? v.as<const char *>() : nullptr;
}
bool ref_cfg_has(const char *key) { return !s_cfg[key].isNull(); }
void ref_cfg_set_str(const char *key, const char *value) { s_cfg[key] = value; }
void ref_cfg_set_u16(const char *key, uint16_t value) { s_cfg[key] = value; }
void ref_cfg_set_bool(const char *key, bool value) { s_cfg[key] = value; }
void ref_cfg_set_u64(const char *key, uint64_t value) { s_cfg[key] = value; }
size_t ref_cfg_save(char *out, size_t cap) { return serializeJson(s_cfg, out, cap); }
}
