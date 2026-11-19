/* The former document building of GET /api/alert/info, /api/influx/info,
 * /api/otp/status, the OTP session response, /api/wifi/scan
 * (main/http_server/) and GET /api/v2/settings (v2/handler_v2_settings.cpp),
 * verbatim (extracted by a script from the commit before HTTP step C3c), on
 * the shims of get_shim.h. Compiled twice: plain and as for the
 * NerdAxe/NerdAxeGamma (pidUseMax). */
#include "get_shim.h"

#ifndef REF_SUFFIX
#define REF_SUFFIX(x) x
#endif

#ifndef NERDAXE
extern "C" char *ref_alert_info(const alert_info_t *snap)
{
    shim_set_alert(snap);
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    char *alertDiscordWebhook = Config::getDiscordWebhook();


    doc["hasWebhook"] = (alertDiscordWebhook && alertDiscordWebhook[0] != '\0');
    doc["watchdogEnable"] = Config::isDiscordWatchdogAlertEnabled() ? 1 : 0;
    doc["blockFoundEnable"] = Config::isDiscordBlockFoundAlertEnabled() ? 1 : 0;
    doc["bestDiffEnable"] = Config::isDiscordBestDiffAlertEnabled() ? 1 : 0;
    doc["coinbaseVerifyEnable"] = Config::isDiscordCoinbaseVerifyAlertEnabled() ? 1 : 0;
    doc["showBlockFoundScreen"] = Config::isShowBlockFoundEnabled() ? 1 : 0;
    return serialize(doc);
}

extern "C" char *ref_influx_info(const influx_info_t *snap)
{
    shim_set_influx(snap);
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    char *influxURL    = Config::getInfluxURL();
    char *influxBucket = Config::getInfluxBucket();
    char *influxOrg    = Config::getInfluxOrg();
    char *influxPrefix = Config::getInfluxPrefix();


    // Fill the JSON object with values
    doc["url"]    = influxURL;
    doc["port"]   = Config::getInfluxPort();
    doc["bucket"] = influxBucket;
    doc["org"]    = influxOrg;
    doc["prefix"] = influxPrefix;
    doc["enabled"] = Config::isInfluxEnabled() ? 1 : 0;
    return serialize(doc);
}

extern "C" char *ref_otp_status(bool enabled)
{
    OtpShim otp(enabled);
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    doc["enabled"] = otp.isEnabled();
    return serialize(doc);
}

extern "C" char *ref_otp_session(const char *token_c, uint32_t ttlSeconds, uint64_t expires_ms)
{
    std::string token = token_c;
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    doc["token"]     = token;
    doc["ttlMs"]     = ttlSeconds * 1000;
    doc["expiresAt"] = (double)expires_ms;
    return serialize(doc);
}

static char *ref_wifi_scan_records(const wifi_ap_record_simple_t *ap_records, uint16_t ap_count)
{
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (uint16_t i = 0; i < ap_count; i++) {
        JsonObject n = networks.add<JsonObject>();
        n["ssid"]     = (const char *) ap_records[i].ssid;
        n["rssi"]     = ap_records[i].rssi;
        n["authmode"] = (int) ap_records[i].authmode;
    }
    return serialize(doc);
}

extern "C" char *ref_wifi_scan(const wifi_network_t *nets, uint16_t count)
{
    std::vector<wifi_ap_record_simple_t> recs(count);
    for (uint16_t i = 0; i < count; i++) {
        memset(&recs[i], 0, sizeof(recs[i]));
        strncpy(recs[i].ssid, nets[i].ssid, sizeof(recs[i].ssid) - 1);
        recs[i].rssi = nets[i].rssi;
        recs[i].authmode = (wifi_auth_mode_t) nets[i].authmode;
    }
    return ref_wifi_scan_records(recs.data(), count);
}
#endif

extern "C" char *REF_SUFFIX(ref_v2_settings)(const v2_settings_t *snap)
{
    shim_set_settings(snap);
    Board* board = SYSTEM_MODULE.getBoard();
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- device identity ---
    doc["asicModel"]   = board->getAsicModel();
    doc["deviceModel"] = board->getDeviceModel();
    doc["version"]     = esp_app_get_description()->version;
    doc["otp"]         = Config::isOTPEnabled();
    doc["apActive"]    = NETWORK.isApActive();

    // --- can ---
    {
        JsonObject can = doc["can"].to<JsonObject>();
        can["hasExtension"] = board->hasCanExtension();
        can["enabled"]      = Config::isCanEnabled();
    }

    // --- asic settings (current + defaults + options merged from /asic endpoint) ---
    doc["frequency"]        = board->getAsicFrequency();
    doc["coreVoltage"]      = board->getAsicVoltageMillis();
    doc["vrFrequency"]      = board->getVrFrequency();
    doc["defaultFrequency"] = board->getDefaultAsicFrequency();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["defaultVrFrequency"] = board->getDefaultVrFrequency();
    doc["ecoFrequency"]     = board->getEcoAsicFrequency();
    doc["ecoCoreVoltage"]   = board->getEcoAsicVoltageMillis();
    doc["absMinCoreVoltage"] = board->getAbsMinAsicVoltageMillis();
    doc["absMaxCoreVoltage"] = board->getAbsMaxAsicVoltageMillis();
    {
        JsonArray arr = doc["frequencyOptions"].to<JsonArray>();
        for (uint32_t f : board->getFrequencyOptions()) arr.add(f);
    }
    {
        JsonArray arr = doc["voltageOptions"].to<JsonArray>();
        for (uint32_t v : board->getVoltageOptions()) arr.add(v);
    }

    // --- stratum / pools ---
    doc["poolMode"]        = Config::getPoolMode();
    doc["poolBalance"]     = Config::getPoolBalance();
    doc["stratumKeep"]    = Config::isStratumKeepaliveEnabled() ? 1 : 0;
    doc["jobInterval"]     = board->getAsicJobIntervalMs();
    doc["stratumDifficulty"] = Config::getStratumDifficulty();
    {
        JsonArray pools = doc["pools"].to<JsonArray>();

        // Pool 0 — primary
        {
            JsonObject pool = pools.add<JsonObject>();
            char *url  = Config::getStratumURL();
            char *user = Config::getStratumUser();
            pool["url"]              = url; 
            pool["port"]             = Config::getStratumPortNumber();
            pool["user"]             = user;
            pool["enonceSubscribe"]  = Config::isStratumEnonceSubscribe();
            pool["tls"]              = Config::isStratumTLS();
            pool["protocol"]         = Config::getStratumProtocol();
            char *sv2 = Config::getSV2AuthorityPubkey();
            pool["sv2AuthorityPubkey"] = sv2;
            safe_free(sv2);
            pool["sv2ChannelType"]   = Config::getSV2ChannelType();
            pool["sv2RequireAuth"]   = Config::isSV2RequireAuth();
            pool["coinbaseVerifyMode"]  = Config::getCoinbaseVerifyMode(0);
            pool["coinbaseMaxFee"]      = Config::getCoinbaseMaxFee(0) / 10.0f;
            pool["coinbaseVerifyForce"] = Config::getCoinbaseVerifyForce(0);
            free(url);
            free(user);
        }

        // Pool 1 — fallback
        {
            JsonObject pool = pools.add<JsonObject>();
            char *url  = Config::getStratumFallbackURL();
            char *user = Config::getStratumFallbackUser();
            pool["url"]              = url;
            pool["port"]             = Config::getStratumFallbackPortNumber();
            pool["user"]             = user;
            pool["enonceSubscribe"]  = Config::isStratumFallbackEnonceSubscribe();
            pool["tls"]              = Config::isStratumFallbackTLS();
            pool["protocol"]         = Config::getFallbackStratumProtocol();
            char *sv2 = Config::getFallbackSV2AuthorityPubkey();
            pool["sv2AuthorityPubkey"] = sv2;
            safe_free(sv2);
            pool["sv2ChannelType"]   = Config::getFallbackSV2ChannelType();
            pool["sv2RequireAuth"]   = Config::isFallbackSV2RequireAuth();
            pool["coinbaseVerifyMode"]  = Config::getCoinbaseVerifyMode(1);
            pool["coinbaseMaxFee"]      = Config::getCoinbaseMaxFee(1) / 10.0f;
            pool["coinbaseVerifyForce"] = Config::getCoinbaseVerifyForce(1);
            free(url);
            free(user);
        }
    }

    // --- fans ---
    {
        JsonArray fans = doc["fans"].to<JsonArray>();
        int numFans = board->getNumFans();
        for (int ch = 0; ch < numFans; ch++) {
            PidSettings* fanPid = board->getPidSettings(ch);
            JsonObject fan = fans.add<JsonObject>();
            fan["label"]       = board->getFanLabel(ch);
            fan["mode"]        = board->getFanMode(ch);
            fan["manualSpeed"] = Config::getFanManualSpeed(ch);
            fan["overheatTemp"] = Config::getFanOverheatTemp(ch);
            JsonObject pid_obj = fan["pid"].to<JsonObject>();
            pid_obj["targetTemp"] = board->isPIDAvailable() ? (int) fanPid->targetTemp : -1;
            pid_obj["p"]          = (float) fanPid->p / 100.0f;
            pid_obj["i"]          = (float) fanPid->i / 100.0f;
            pid_obj["d"]          = (float) fanPid->d / 100.0f;
        }
    }
    doc["invertFanPolarity"] = board->isInvertFanPolarityEnabled() ? 1 : 0;
#if defined(NERDAXE) || defined(NERDAXEGAMMA)
    doc["pidUseMax"]         = Config::isFanPidUseMax();
#endif

    // --- network ---
    {
        char *hostname = Config::getHostname();
        char *ssid     = Config::getWifiSSID();
        doc["hostname"] = hostname;
        doc["ssid"]     = ssid;
        free(hostname);
        free(ssid);
    }

    // --- mempool ---
    {
        doc["mempoolCustom"] = Config::isMempoolCustom();
        char *mempoolUrl = Config::getMempoolUrl();
        doc["mempoolUrl"] = mempoolUrl;
        free(mempoolUrl);
    }

    // --- display ---
    doc["flipScreen"]    = board->isFlipScreenEnabled() ? 1 : 0;
    doc["invertScreen"]  = Config::isInvertScreenEnabled() ? 1 : 0;
    doc["autoScreenOff"] = Config::isAutoScreenOffEnabled() ? 1 : 0;
    return serialize(doc);
}
