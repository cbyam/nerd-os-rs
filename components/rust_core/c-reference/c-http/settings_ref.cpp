/* The former JSON handling of the settings endpoints, verbatim from
 * main/http_server/handler_system.cpp (PATCH_update_settings),
 * main/stratum/stratum_manager.cpp and stratum_manager_dual_pool.cpp
 * (saveSettings) and main/http_server/v2/handler_v2_settings.cpp
 * (PATCH_V2_settings), before HTTP step H2. Only the code between parsing
 * and the subsystem reloads; the config writes go to the recording
 * cfgSet*() of cfg_stub.cpp. */
#include <cmath>
#include <cstdint>
#include <cstring>

#include "ArduinoJson.h"
#include "nvs_config.h"

#define VR_FREQUENCY_ENABLED
#define ESP_LOGI(tag, ...) ((void) (tag))

// StratumManager::saveSettings()
static void stratum_save_settings(const JsonDocument &doc)
{
    if (doc["poolMode"].is<uint16_t>()) {
        Config::setPoolMode(doc["poolMode"].as<uint16_t>());
    }
    if (doc["stratumURL"].is<const char*>()) {
        Config::setStratumURL(doc["stratumURL"].as<const char*>());
    }
    if (doc["stratumUser"].is<const char*>()) {
        Config::setStratumUser(doc["stratumUser"].as<const char*>());
    }
    if (doc["stratumPassword"].is<const char*>()) {
        Config::setStratumPass(doc["stratumPassword"].as<const char*>());
    }
    if (doc["stratumPort"].is<uint16_t>()) {
        Config::setStratumPortNumber(doc["stratumPort"].as<uint16_t>());
    }
    if (doc["stratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumEnonceSubscribe(doc["stratumEnonceSubscribe"].as<bool>());
    }
    if (doc["stratumTLS"].is<bool>()) {
        Config::setStratumTLS(doc["stratumTLS"].as<bool>());
    }
    if (doc["fallbackStratumURL"].is<const char*>()) {
        Config::setStratumFallbackURL(doc["fallbackStratumURL"].as<const char*>());
    }
    if (doc["fallbackStratumUser"].is<const char*>()) {
        Config::setStratumFallbackUser(doc["fallbackStratumUser"].as<const char*>());
    }
    if (doc["fallbackStratumPassword"].is<const char*>()) {
        Config::setStratumFallbackPass(doc["fallbackStratumPassword"].as<const char*>());
    }
    if (doc["fallbackStratumPort"].is<uint16_t>()) {
        Config::setStratumFallbackPortNumber(doc["fallbackStratumPort"].as<uint16_t>());
    }
    if (doc["fallbackStratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumFallbackEnonceSubscribe(doc["fallbackStratumEnonceSubscribe"].as<bool>());
    }
    if (doc["fallbackStratumTLS"].is<bool>()) {
        Config::setStratumFallbackTLS(doc["fallbackStratumTLS"].as<bool>());
    }
    // SV2 settings
    if (doc["stratumProtocol"].is<uint16_t>()) {
        Config::setStratumProtocol(doc["stratumProtocol"].as<uint16_t>());
    }
    if (doc["fallbackStratumProtocol"].is<uint16_t>()) {
        Config::setFallbackStratumProtocol(doc["fallbackStratumProtocol"].as<uint16_t>());
    }
    if (doc["sv2AuthorityPubkey"].is<const char*>()) {
        Config::setSV2AuthorityPubkey(doc["sv2AuthorityPubkey"].as<const char*>());
    }
    if (doc["fallbackSv2AuthorityPubkey"].is<const char*>()) {
        Config::setFallbackSV2AuthorityPubkey(doc["fallbackSv2AuthorityPubkey"].as<const char*>());
    }
    if (doc["sv2ChannelType"].is<uint16_t>()) {
        Config::setSV2ChannelType(doc["sv2ChannelType"].as<uint16_t>());
    }
    if (doc["fallbackSv2ChannelType"].is<uint16_t>()) {
        Config::setFallbackSV2ChannelType(doc["fallbackSv2ChannelType"].as<uint16_t>());
    }
}

// StratumManagerDualPool::saveSettings() (without its mutex)
static void dual_pool_save_settings(const JsonDocument &doc)
{
    if (doc["poolBalance"].is<uint16_t>()) {
        Config::setPoolBalance(doc["poolBalance"].as<uint16_t>());
    }
    stratum_save_settings(doc);
    Config::flush();
}

extern "C" {
// returns false for invalid JSON (getJsonData() sent 400)
bool ref_settings_v1(const char *buf, bool dual_pool)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        return false;
    }

    if (doc["ssid"].is<const char*>()) {
        Config::setWifiSSID(doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        Config::setWifiPass(doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        Config::setHostname(doc["hostname"].as<const char*>());
    }
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t coreVoltage = doc["coreVoltage"].as<uint16_t>();
        if (coreVoltage > 0) {
            Config::setAsicVoltage(coreVoltage);
        }
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t frequency = doc["frequency"].as<uint16_t>();
        if (frequency > 0) {
            Config::setAsicFrequency(frequency);
        }
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t jobInterval = doc["jobInterval"].as<uint16_t>();
        if (jobInterval > 0) {
            Config::setAsicJobInterval(jobInterval);
        }
    }
    if (doc["stratumDifficulty"].is<uint32_t>()) {
        Config::setStratumDifficulty(doc["stratumDifficulty"].as<uint32_t>());
    }
    if (doc["flipscreen"].is<bool>()) {
        Config::setFlipScreen(doc["flipscreen"].as<bool>());
    }
    if (doc["overheat_temp"].is<uint16_t>()) {
        Config::setOverheatTemp(doc["overheat_temp"].as<uint16_t>());
    }
    if (doc["invertscreen"].is<bool>()) {
        Config::setInvertScreen(doc["invertscreen"].as<bool>());
    }
    if (doc["invertfanpolarity"].is<bool>()) {
        Config::setFanPolarity(doc["invertfanpolarity"].as<bool>());
    }
    if (doc["autofanspeed"].is<uint16_t>()) {
        Config::setTempControlMode(doc["autofanspeed"].as<uint16_t>());
    }
    if (doc["manualFanSpeed"].is<uint16_t>()) {
        Config::setFanSpeed(doc["manualFanSpeed"].as<uint16_t>());
    }
    if (doc["autoscreenoff"].is<bool>()) {
        Config::setAutoScreenOff(doc["autoscreenoff"].as<bool>());
    }
    if (doc["stratum_keep"].is<bool>() || doc["stratum_keep"].is<int>()) {
        bool value = doc["stratum_keep"].as<int>() != 0;
        Config::setStratumKeepaliveEnabled(value);
        ESP_LOGI("system", "stratum_keep updated via WebUI: %s", value ? "ENABLED" : "DISABLED");
    }
    if (doc["pidTargetTemp"].is<uint16_t>()) {
        Config::setPidTargetTemp(doc["pidTargetTemp"].as<uint16_t>());
    }
    if (doc["pidP"].is<float>()) {
        Config::setPidP((uint16_t) (doc["pidP"].as<float>() * 100.0f));
    }
    if (doc["pidI"].is<float>()) {
        Config::setPidI((uint16_t) (doc["pidI"].as<float>() * 100.0f));
    }
    if (doc["pidD"].is<float>()) {
        Config::setPidD((uint16_t) (doc["pidD"].as<float>() * 100.0f));
    }
#ifdef VR_FREQUENCY_ENABLED
    if (doc["vrFrequency"].is<uint32_t>()) {
        Config::setVrFrequency(doc["vrFrequency"].as<uint32_t>());
    }
#endif

    // Per-channel fan settings: fans[0] maps to ch0 NVS keys, fans[1] to ch1 NVS keys
    if (doc["fans"].is<JsonArray>()) {
        JsonArray fans = doc["fans"].as<JsonArray>();
        int ch = 0;
        for (JsonObject fan : fans) {
            if (ch > 1) break;
            if (fan["mode"].is<uint16_t>())
                Config::setFanMode(ch, fan["mode"].as<uint16_t>());
            if (fan["manualSpeed"].is<uint16_t>())
                Config::setFanManualSpeed(ch, fan["manualSpeed"].as<uint16_t>());
            if (fan["overheatTemp"].is<uint16_t>())
                Config::setFanOverheatTemp(ch, fan["overheatTemp"].as<uint16_t>());
            if (fan["pid"].is<JsonObject>()) {
                JsonObject p = fan["pid"].as<JsonObject>();
                if (p["targetTemp"].is<uint16_t>())
                    Config::setFanPidTargetTemp(ch, p["targetTemp"].as<uint16_t>());
                if (p["p"].is<float>())
                    Config::setFanPidP(ch, (uint16_t) (p["p"].as<float>() * 100.0f));
                if (p["i"].is<float>())
                    Config::setFanPidI(ch, (uint16_t) (p["i"].as<float>() * 100.0f));
                if (p["d"].is<float>())
                    Config::setFanPidD(ch, (uint16_t) (p["d"].as<float>() * 100.0f));
            }
            ch++;
        }
    }

    // save stratum settings
    if (dual_pool) {
        dual_pool_save_settings(doc);
    } else {
        stratum_save_settings(doc);
    }
    return true;
}

bool ref_settings_v2(const char *buf, bool *verify_changed)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        return false;
    }

    // --- network ---
    if (doc["ssid"].is<const char*>()) {
        Config::setWifiSSID(doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        Config::setWifiPass(doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        Config::setHostname(doc["hostname"].as<const char*>());
    }

    // --- ASIC settings ---
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t v = doc["coreVoltage"].as<uint16_t>();
        if (v > 0) Config::setAsicVoltage(v);
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t f = doc["frequency"].as<uint16_t>();
        if (f > 0) Config::setAsicFrequency(f);
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t ji = doc["jobInterval"].as<uint16_t>();
        if (ji > 0) Config::setAsicJobInterval(ji);
    }
    if (doc["stratumDifficulty"].is<uint32_t>()) {
        Config::setStratumDifficulty(doc["stratumDifficulty"].as<uint32_t>());
    }
    if (doc["vrFrequency"].is<uint32_t>()) {
        Config::setVrFrequency(doc["vrFrequency"].as<uint32_t>());
    }

    // --- display ---
    if (doc["flipScreen"].is<bool>()) {
        Config::setFlipScreen(doc["flipScreen"].as<bool>());
    }
    if (doc["invertScreen"].is<bool>()) {
        Config::setInvertScreen(doc["invertScreen"].as<bool>());
    }
    if (doc["autoScreenOff"].is<bool>()) {
        Config::setAutoScreenOff(doc["autoScreenOff"].as<bool>());
    }
    if (doc["mempoolCustom"].is<bool>()) {
        Config::setMempoolCustom(doc["mempoolCustom"].as<bool>());
    }
    if (doc["mempoolUrl"].is<const char*>()) {
        Config::setMempoolUrl(doc["mempoolUrl"].as<const char*>());
    }
    if (doc["invertFanPolarity"].is<bool>()) {
        Config::setFanPolarity(doc["invertFanPolarity"].as<bool>());
    }
#if defined(NERDAXE) || defined(NERDAXEGAMMA)
    if (doc["pidUseMax"].is<bool>()) {
        Config::setFanPidUseMax(doc["pidUseMax"].as<bool>());
    }
#endif

    // --- misc ---
    if (doc["stratumKeep"].is<bool>() || doc["stratumKeep"].is<int>()) {
        bool value = doc["stratumKeep"].as<int>() != 0;
        Config::setStratumKeepaliveEnabled(value);
    }
    if (doc["canMaster"].is<bool>() || doc["canMaster"].is<int>()) {
        bool value = doc["canMaster"].as<int>() != 0;
        Config::setCanEnabled(value);
    }

    // --- fans[] ---
    if (doc["fans"].is<JsonArray>()) {
        JsonArray fans = doc["fans"].as<JsonArray>();
        int ch = 0;
        for (JsonObject fan : fans) {
            if (ch > 1) break;
            if (fan["mode"].is<uint16_t>())
                Config::setFanMode(ch, fan["mode"].as<uint16_t>());
            if (fan["manualSpeed"].is<uint16_t>())
                Config::setFanManualSpeed(ch, fan["manualSpeed"].as<uint16_t>());
            if (fan["overheatTemp"].is<uint16_t>())
                Config::setFanOverheatTemp(ch, fan["overheatTemp"].as<uint16_t>());
            if (fan["pid"].is<JsonObject>()) {
                JsonObject p = fan["pid"].as<JsonObject>();
                if (p["targetTemp"].is<uint16_t>())
                    Config::setFanPidTargetTemp(ch, p["targetTemp"].as<uint16_t>());
                if (p["p"].is<float>())
                    Config::setFanPidP(ch, (uint16_t)(p["p"].as<float>() * 100.0f));
                if (p["i"].is<float>())
                    Config::setFanPidI(ch, (uint16_t)(p["i"].as<float>() * 100.0f));
                if (p["d"].is<float>())
                    Config::setFanPidD(ch, (uint16_t)(p["d"].as<float>() * 100.0f));
            }
            ch++;
        }
    }

    // --- pools[] ---
    if (doc["poolMode"].is<uint16_t>()) {
        Config::setPoolMode(doc["poolMode"].as<uint16_t>());
    }
    if (doc["poolBalance"].is<uint16_t>()) {
        Config::setPoolBalance(doc["poolBalance"].as<uint16_t>());
    }

    bool verifyChanged[2] = {false, false};

    if (doc["pools"].is<JsonArray>()) {
        JsonArray pools = doc["pools"].as<JsonArray>();

        for (int i = 0; i < (int)pools.size() && i < 2; i++) {
            JsonObject pool = pools[i].as<JsonObject>();

            if (pool["url"].is<const char*>()) {
                if (i == 0) Config::setStratumURL(pool["url"].as<const char*>());
                else        Config::setStratumFallbackURL(pool["url"].as<const char*>());
            }
            if (pool["port"].is<uint16_t>()) {
                if (i == 0) Config::setStratumPortNumber(pool["port"].as<uint16_t>());
                else        Config::setStratumFallbackPortNumber(pool["port"].as<uint16_t>());
            }
            if (pool["user"].is<const char*>()) {
                if (i == 0) Config::setStratumUser(pool["user"].as<const char*>());
                else        Config::setStratumFallbackUser(pool["user"].as<const char*>());
            }
            if (pool["password"].is<const char*>()) {
                if (i == 0) Config::setStratumPass(pool["password"].as<const char*>());
                else        Config::setStratumFallbackPass(pool["password"].as<const char*>());
            }
            if (pool["enonceSubscribe"].is<bool>()) {
                if (i == 0) Config::setStratumEnonceSubscribe(pool["enonceSubscribe"].as<bool>());
                else        Config::setStratumFallbackEnonceSubscribe(pool["enonceSubscribe"].as<bool>());
            }
            if (pool["tls"].is<bool>()) {
                if (i == 0) Config::setStratumTLS(pool["tls"].as<bool>());
                else        Config::setStratumFallbackTLS(pool["tls"].as<bool>());
            }
            if (pool["protocol"].is<uint16_t>()) {
                if (i == 0) Config::setStratumProtocol(pool["protocol"].as<uint16_t>());
                else        Config::setFallbackStratumProtocol(pool["protocol"].as<uint16_t>());
            }
            if (pool["sv2AuthorityPubkey"].is<const char*>()) {
                if (i == 0) Config::setSV2AuthorityPubkey(pool["sv2AuthorityPubkey"].as<const char*>());
                else        Config::setFallbackSV2AuthorityPubkey(pool["sv2AuthorityPubkey"].as<const char*>());
            }
            if (pool["sv2ChannelType"].is<uint16_t>()) {
                if (i == 0) Config::setSV2ChannelType(pool["sv2ChannelType"].as<uint16_t>());
                else        Config::setFallbackSV2ChannelType(pool["sv2ChannelType"].as<uint16_t>());
            }
            if (pool["sv2RequireAuth"].is<bool>()) {
                if (i == 0) Config::setSV2RequireAuth(pool["sv2RequireAuth"].as<bool>());
                else        Config::setFallbackSV2RequireAuth(pool["sv2RequireAuth"].as<bool>());
            }

            // Coinbase verification
            if (pool["coinbaseVerifyMode"].is<uint16_t>()) {
                verifyChanged[i] |= Config::getCoinbaseVerifyMode(i) != pool["coinbaseVerifyMode"].as<uint16_t>();
                Config::setCoinbaseVerifyMode(i, pool["coinbaseVerifyMode"].as<uint16_t>());
            }
            if (pool["coinbaseMaxFee"].is<float>()) {
                uint16_t newVal = (uint16_t)roundf(pool["coinbaseMaxFee"].as<float>() * 10.0f);
                verifyChanged[i] |= Config::getCoinbaseMaxFee(i) != newVal;
                Config::setCoinbaseMaxFee(i, newVal);
            }
            if (pool["coinbaseVerifyForce"].is<bool>()) {
                verifyChanged[i] |= Config::getCoinbaseVerifyForce(i) != pool["coinbaseVerifyForce"].as<bool>();
                Config::setCoinbaseVerifyForce(i, pool["coinbaseVerifyForce"].as<bool>());
            }
        }
    }

    verify_changed[0] = verifyChanged[0];
    verify_changed[1] = verifyChanged[1];
    return true;
}
}
