/* The former StratumTaskV2::loadAuthorityPubkey() (verbatim from HEAD
 * before the port) with libbase58 (c/base58.c, the same code) on shims: the
 * config key is the string under test. */
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" bool b58tobin(void *bin, size_t *binsz, const char *b58, size_t b58sz);

#define ESP_LOGE(tag, ...) ((void) (tag))
#define ESP_LOGI(tag, ...) ((void) (tag))
static const char *TAG = "stratum_v2";
static const char *g_key;
static void safe_free(char *p) { free(p); }
struct ConfigShim {
    bool isPrimary() { return true; }
};
static ConfigShim g_cfg;
static ConfigShim *m_config = &g_cfg;
namespace Config {
static char *getSV2AuthorityPubkey() { return strdup(g_key); }
static char *getFallbackSV2AuthorityPubkey() { return strdup(g_key); }
} // namespace Config

static bool old_load(uint8_t out[32])
{
    char *b58_key = m_config->isPrimary()
        ? Config::getSV2AuthorityPubkey()
        : Config::getFallbackSV2AuthorityPubkey();

    if (!b58_key || strlen(b58_key) == 0) {
        safe_free(b58_key);
        return false;
    }

    // SV2 authority pubkey format: base58check(0x0001_LE + 32_byte_xonly_pubkey)
    // Decoded: 2-byte version + 32-byte pubkey + 4-byte checksum = 38 bytes
    uint8_t decoded[64];
    size_t decoded_len = sizeof(decoded);

    if (!b58tobin(decoded, &decoded_len, b58_key, 0)) {
        ESP_LOGE(TAG, "Failed to decode base58 authority pubkey");
        safe_free(b58_key);
        return false;
    }
    safe_free(b58_key);

    if (decoded_len != 38) {
        ESP_LOGE(TAG, "Invalid decoded length: %zu (expected 38)", decoded_len);
        return false;
    }

    // b58tobin right-aligns data in the buffer
    uint8_t *data = decoded + (sizeof(decoded) - decoded_len);

    // Verify version (0x0001 in little-endian)
    if (data[0] != 0x01 || data[1] != 0x00) {
        ESP_LOGE(TAG, "Invalid key version: 0x%02x%02x (expected 0x0100)", data[1], data[0]);
        return false;
    }

    memcpy(out, data + 2, 32);
    ESP_LOGI(TAG, "Successfully decoded base58 authority pubkey");
    return true;
}

extern "C" bool ref_sv2_authority_key(const char *b58, uint8_t out[32])
{
    g_key = b58;
    return old_load(out);
}
