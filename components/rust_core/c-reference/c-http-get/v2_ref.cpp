/* The former document building of GET /api/v2/dashboard, /api/v2/system and
 * /api/v2/identify (main/http_server/v2/), verbatim (extracted by a script
 * from the commit before HTTP step C3b; vrOverheatTemp from upstream
 * v1.1.3-alpha.1), on the shims of get_shim.h. */
#include "get_shim.h"

extern "C" char *ref_dashboard(const dashboard_t *snap, uint64_t start_timestamp, uint64_t current_timestamp,
                               uint32_t history_limit, bool history_requested, uint64_t history_span_ms)
{
    shim_set_dash(snap);
    Board*   board   = SYSTEM_MODULE.getBoard();
    HistoryShim* history = SYSTEM_MODULE.getHistory();
    bool     shutdown = POWER_MANAGEMENT_MODULE.isShutdown();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- system ---
    {
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["uptime"]       = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
        sys["shutdown"]     = shutdown;
        sys["boardError"]   = (int) SYSTEM_MODULE.getBoardError();
        sys["overheatTemp"] = Config::getOverheatTemp();
        // VR shutdown runs on fan channel 1, so it only exists on boards with two channels
        sys["vrOverheatTemp"] = (board->getNumFans() > 1) ? Config::getFanOverheatTemp(1) : 0;
    }

    // --- performance ---
    {
        JsonObject perf = doc["performance"].to<JsonObject>();
        perf["hashRateTimestamp"] = history->getCurrentTimestamp();
        perf["hashRate"]     = !shutdown ? SYSTEM_MODULE.getCurrentHashrate()    : 0.0;
        perf["hashRate1m"]  = !shutdown ? history->getCurrentHashrate1m()       : 0.0;
        perf["hashRate10m"] = !shutdown ? history->getCurrentHashrate10m()      : 0.0;
        perf["hashRate1h"]  = !shutdown ? history->getCurrentHashrate1h()       : 0.0;
        perf["hashRate1d"]  = !shutdown ? history->getCurrentHashrate1d()       : 0.0;
        perf["bestDiff"]        = STRATUM_MANAGER->getBestDiff();
        perf["bestSessionDiff"] = STRATUM_MANAGER->getBestSessionDiff();
        perf["sharesAccepted"]  = STRATUM_MANAGER->getSharesAccepted();
        perf["sharesRejected"]  = STRATUM_MANAGER->getSharesRejected();
        perf["frequency"]       = board->getAsicFrequency();
        perf["asicCount"]       = board->getAsicCount();
        perf["smallCoreCount"]  = board->getAsics() ? board->getAsics()->getSmallCoreCount() : 0;

        // per-chip valid hashrate + hardware error hashrate (both GH/s) from the
        // ASIC counters. total = hashRate + errorHashRate.
        JsonArray chips = perf["chips"].to<JsonArray>();
        for (int i = 0; i < board->getAsicCount(); i++) {
            JsonObject chip = chips.add<JsonObject>();
            chip["hashRate"]      = !shutdown ? HASHRATE_MONITOR.getChipHashrate(i)  : 0.0;
            chip["errorHashRate"] = !shutdown ? HASHRATE_MONITOR.getChipErrorRate(i) : 0.0;
        }
    }

    // --- power ---
    {
        JsonObject pwr = doc["power"].to<JsonObject>();
        pwr["watts"]             = POWER_MANAGEMENT_MODULE.getPower();
        pwr["min"]               = board->getMinPin();
        pwr["max"]               = board->getMaxPin();
        pwr["voltage"]           = POWER_MANAGEMENT_MODULE.getVoltage() / 1000.0f; // V
        pwr["voltageMin"]        = board->getMinVin();
        pwr["voltageMax"]        = board->getMaxVin();
        pwr["currentA"]          = POWER_MANAGEMENT_MODULE.getCurrent() / 1000.0f; // A
        pwr["currentAMin"]       = board->getMinCurrentA();
        pwr["currentAMax"]       = board->getMaxCurrentA();
        pwr["coreVoltageActual"] = board->getVout(); // V
    }

    // --- thermal ---
    {
        JsonObject therm = doc["thermal"].to<JsonObject>();
        therm["asicTemp"]  = POWER_MANAGEMENT_MODULE.getChipTempMax();
        therm["vrTemp"]    = POWER_MANAGEMENT_MODULE.getVRTemp();
        therm["vrTempInt"] = POWER_MANAGEMENT_MODULE.getVRTempInt();
        {
            JsonArray asic_temps = therm["asicTemps"].to<JsonArray>();
            for (int i = 0; i < board->getAsicCount(); i++) {
                asic_temps.add(board->getChipTemp(i));
            }
        }
        {
            JsonArray fans = therm["fans"].to<JsonArray>();
            for (int ch = 0; ch < board->getNumFans(); ch++) {
                JsonObject fan = fans.add<JsonObject>();
                fan["speed"] = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
                fan["rpm"]   = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
            }
        }
    }

    // --- stratum ---
    {
        JsonObject stratum = doc["stratum"].to<JsonObject>();
        STRATUM_MANAGER->getManagerInfoJson(stratum, true);

        // Enrich each pool entry with host / port / user from NVS config
        char *urls[2]  = { Config::getStratumURL(), Config::getStratumFallbackURL() };
        char *users[2] = { Config::getStratumUser(), Config::getStratumFallbackUser() };
        int   ports[2] = { (int) Config::getStratumPortNumber(), (int) Config::getStratumFallbackPortNumber() };

        // the managers report both pools in config order (0 = primary,
        // 1 = fallback/secondary), so the array index is the config index
        JsonArray pools = stratum["pools"].as<JsonArray>();
        for (int i = 0; i < (int) pools.size() && i < 2; i++) {
            JsonObject pool = pools[i].as<JsonObject>();
            pool["host"] = urls[i];
            pool["port"] = ports[i];
            pool["user"] = users[i];
        }

        for (int i = 0; i < 2; i++) {
            if (urls[i])  free(urls[i]);
            if (users[i]) free(users[i]);
        }
    }

    // --- can ---
    {
        JsonObject can = doc["can"].to<JsonObject>();
        can["hasExtension"] = board->hasCanExtension();
        can["enabled"]      = Config::isCanEnabled();
    }

    // --- coinbase ---
    {
        JsonObject coinbase = doc["coinbase"].to<JsonObject>();

        JsonArray blockHeaders = coinbase["blockHeaders"].to<JsonArray>();
        for (int p = 0; p < 2; p++) {
            const coinbase_result_t cb = STRATUM_MANAGER->getCoinbaseResult(p);
            if (cb.block_height > 0) {
                JsonObject bh = blockHeaders.add<JsonObject>();
                bh["pool"]              = p;
                bh["blockHeight"]       = cb.block_height;
                bh["networkDifficulty"] = cb.network_difficulty;
                // as const char[N] ArduinoJson would keep a pointer to the
                // loop-local copy with length N-1, like a string literal
                bh["scriptSig"]         = (const char *) cb.scriptsig;
                if (Config::getCoinbaseVerifyMode(p) > 0) {
                    bh["coinbaseValueTotalSatoshis"] = cb.total_value_satoshis;
                    bh["coinbaseValueUserSatoshis"]  = cb.user_value_satoshis;
                    bh["verificationOk"]         = STRATUM_MANAGER->getVerificationOk(p);
                    bh["verificationFailCount"]  = STRATUM_MANAGER->getVerificationFailCount(p);
                    bh["verificationCheckCount"] = STRATUM_MANAGER->getVerificationCheckCount(p);
                }
            }
        }

        JsonArray cbPools = coinbase["pools"].to<JsonArray>();
        for (int p = 0; p < 2; p++) {
            JsonObject cp = cbPools.add<JsonObject>();
            cp["mode"]   = Config::getCoinbaseVerifyMode(p);
            cp["maxFee"] = Config::getCoinbaseMaxFee(p) / 10.0f;
            cp["force"]  = Config::getCoinbaseVerifyForce(p);
        }
    }

    // --- history (optional, same query params as /api/system/info) ---
    if (!shutdown && history_requested) {
        uint64_t end_timestamp = start_timestamp + history_span_ms;
        JsonObject json_history = doc["history"].to<JsonObject>();
        history->exportHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp, history_limit);
    }

    return serialize(doc);
}

extern "C" char *ref_v2_system(const v2_system_t *snap)
{
    shim_set_v2sys(snap);
    Board *board = SYSTEM_MODULE.getBoard();

    char *hostname = Config::getHostname();
    char *ssid     = Config::getWifiSSID();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Device identity
    doc["deviceModel"] = board->getDeviceModel();
    doc["asicModel"]   = board->getAsicModel();
    doc["version"]     = esp_app_get_description()->version;

    // Uptime & reset
    doc["uptimeSeconds"]    = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]  = SYSTEM_MODULE.getLastResetReason();

    // Network
    JsonObject network = doc["network"].to<JsonObject>();
    network["hostname"]   = hostname;
    network["ssid"]       = ssid;
    network["macAddr"]    = SYSTEM_MODULE.getMacAddress();
    network["ipAddr"]     = SYSTEM_MODULE.getIPAddress();
    network["wifiStatus"] = SYSTEM_MODULE.getWifiStatus();
    network["wifiRSSI"]   = SYSTEM_MODULE.get_wifi_rssi();

    // Memory
    JsonObject memory = doc["memory"].to<JsonObject>();
    memory["freeHeap"]    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    memory["freeHeapInt"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    free(hostname);
    free(ssid);

    return serialize(doc);
}

extern "C" char *ref_v2_identify(const v2_identify_t *snap)
{
    shim_set_identify(snap);
    Board* board = SYSTEM_MODULE.getBoard();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    doc["deviceModel"] = board->getDeviceModel();
    doc["defaultTheme"] = board->getDefaultTheme();
    doc["otp"]          = Config::isOTPEnabled();
    doc["apActive"]     = NETWORK.isApActive();

    // Whether this build compiled in the panic core dump download endpoint.
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
    doc["coreDumpDownload"] = true;
#else
    doc["coreDumpDownload"] = false;
#endif

    JsonObject can = doc["can"].to<JsonObject>();
    can["enabled"] = Config::isCanEnabled();

    return serialize(doc);
}
