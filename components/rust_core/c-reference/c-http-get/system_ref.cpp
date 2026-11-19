/* The former document building of GET /api/system/info and
 * GET /api/system/asic (main/http_server/handler_system.cpp), of
 * StratumManager::getManagerInfoJson() (main/stratum/stratum_manager.cpp) and
 * of History::exportHistoryData() (main/history.cpp), verbatim (extracted by
 * a script from the commit before HTTP step C3a), on shim objects
 * (get_shim.h) that return the values of the differential test's snapshot. */
#include "get_shim.h"

namespace {
struct Arrays {
    JsonArray hashrate_1m, hashrate_10m, hashrate_1h, hashrate_1d, timestamps, vregTemps, asicTemps;
};
} // namespace

// fills a JsonObject with the history between the timestamps (client time)
void HistoryShim::exportHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp,
                                uint64_t current_timestamp, uint32_t limit)
{
    Arrays a = {
        .hashrate_1m = json_history["hashrate_1m"].to<JsonArray>(),
        .hashrate_10m = json_history["hashrate_10m"].to<JsonArray>(),
        .hashrate_1h = json_history["hashrate_1h"].to<JsonArray>(),
        .hashrate_1d = json_history["hashrate_1d"].to<JsonArray>(),
        .timestamps = json_history["timestamps"].to<JsonArray>(),
        .vregTemps = json_history["vregTemp"].to<JsonArray>(),
        .asicTemps = json_history["asicTemp"].to<JsonArray>(),
    };

    bool hasMore = history_export(
        start_timestamp, end_timestamp, current_timestamp, limit, esp_timer_get_time() / 1000ULL,
        [](void *ctx, const history_row_t *r) {
            Arrays *a = static_cast<Arrays *>(ctx);
            a->hashrate_1m.add(r->hashrate_1m);
            a->hashrate_10m.add(r->hashrate_10m);
            a->hashrate_1h.add(r->hashrate_1h);
            a->hashrate_1d.add(r->hashrate_1d);
            a->vregTemps.add(r->vreg_temp);
            a->asicTemps.add(r->asic_temp);
            a->timestamps.add(r->timestamp);
        },
        &a);

    // base timestamp for reference
    json_history["timestampBase"] = start_timestamp;
    json_history["hasMore"] = hasMore;
}

void StratumManagerShim::getManagerInfoJson(JsonObject &obj, bool verbose)
{
    PThreadGuard lock(m_mutex);

    obj["poolMode"] = Config::getPoolMode();
    obj["activePoolMode"] = getPoolMode();

    // this belongs to dual pool but we need it here for the web UI
    // running at pri/fb mode and switching to dual pool wouldn't
    // return the config value until next boot
    obj["poolBalance"] = Config::getPoolBalance();

    obj["totalBestDiff"] = getBestDiff();

    if (isFallback()) {
        obj["usingFallback"] = isUsingFallback();
    }

    JsonArray arr = obj["pools"].to<JsonArray>();

    // always report both pool configs, array order matches the config
    // (0 = primary, 1 = fallback); in failover mode "active" marks the one
    // currently mining
    int selected = selectedPool();
    for (int i = 0; i < 2; i++) {
        bool active = isDualPool() || i == selected;

        // the legacy v1 /info reports only the active pool in failover mode
        // (single entry), matching the pre-v1.1.0 shape external clients
        // (Blocktrainer terminal) rely on; the v2 dashboard (verbose) still
        // gets both pool configs
        if (!verbose && !active) {
            continue;
        }

        JsonObject pool = arr.add<JsonObject>();

        pool["connected"] = m_stratumTasks[i] ? m_stratumTasks[i]->m_isConnected : false;

        // failover mode: the session stats are shared between both pools,
        // reported on the active entry (below)
        pool_stats_pool_t st = getPoolStats(i);
        if (isDualPool()) {
            pool["poolDifficulty"] = st.pool_difficulty;
            pool["networkDifficulty"] = st.network_difficulty;
            pool["poolDiffErr"] = pool_policy_pool_diff_err(m_policy, i);
            pool["accepted"] = st.accepted;
            pool["rejected"] = st.rejected;
        }

        pool["pingRtt"] = m_pingTasks[i] ? m_pingTasks[i]->get_last_ping_rtt() : 0;
        pool["pingLoss"] = m_pingTasks[i] ? m_pingTasks[i]->get_recent_ping_loss() : 0;

        if (isDualPool()) {
            pool["bestDiff"] = st.best_session;
        }

        // dashboard-only fields — kept out of the legacy v1 /info so its
        // response stays small and shape-compatible with external clients
        if (verbose) {
            pool["active"] = active;
            pool["verifyBlocked"] = getVerifyBlockedReason(i) ? getVerifyBlockedReason(i) : "";
            pool["activeProtocol"] = m_stratumConfig[i] ? (int) m_stratumConfig[i]->getProtocol() : 0;
            pool["encrypted"] =
                m_stratumConfig[i] ? (m_stratumConfig[i]->isSV2() || m_stratumConfig[i]->isTLS()) : false;
        }

        if (!isDualPool() && active) {
            pool_stats_summary_t sum = getStats();
            pool["poolDifficulty"] = sum.pool_difficulty;
            pool["networkDifficulty"] = sum.network_difficulty;
            pool["poolDiffErr"] = false;
            pool["accepted"] = sum.accepted;
            pool["rejected"] = sum.rejected;
            pool["bestDiff"] = sum.best_session;
        }
    }
}


extern "C" char *ref_system_info(const system_info_t *snap, uint64_t start_timestamp, uint64_t current_timestamp,
                                 uint32_t history_limit, bool history_requested, uint64_t history_span_ms)
{
    shim_set(snap);
    Board *board = SYSTEM_MODULE.getBoard();
    HistoryShim *history = SYSTEM_MODULE.getHistory();
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    bool shutdown = POWER_MANAGEMENT_MODULE.isShutdown();

    // Get configuration strings from NVS
    char *ssid               = Config::getWifiSSID();
    char *hostname           = Config::getHostname();
    char *stratumURL         = Config::getStratumURL();
    char *stratumUser        = Config::getStratumUser();
    char *fallbackStratumURL = Config::getStratumFallbackURL();
    char *fallbackStratumUser= Config::getStratumFallbackUser();

    char *sv2_auth = Config::getSV2AuthorityPubkey();
    char *fb_sv2_auth = Config::getFallbackSV2AuthorityPubkey();


    // static
    doc["asicCount"]          = board->getAsicCount();
    doc["smallCoreCount"]     = (board->getAsics()) ? board->getAsics()->getSmallCoreCount() : 0;
    doc["deviceModel"]        = board->getDeviceModel();
    doc["hostip"]             = SYSTEM_MODULE.getIPAddress();
    doc["macAddr"]            = SYSTEM_MODULE.getMacAddress();
    doc["wifiRSSI"]           = SYSTEM_MODULE.get_wifi_rssi();

    // dashboard
    doc["power"]              = POWER_MANAGEMENT_MODULE.getPower();
    doc["maxPower"]           = board->getMaxPin();
    doc["minPower"]           = board->getMinPin();
    doc["maxVoltage"]         = board->getMaxVin();
    doc["minVoltage"]         = board->getMinVin();
    doc["current"]            = POWER_MANAGEMENT_MODULE.getCurrent();           // mA (raw)
    doc["currentA"]           = POWER_MANAGEMENT_MODULE.getCurrent() / 1000.0f; // A (UI)
    doc["minCurrentA"]        = board->getMinCurrentA(); // A
    doc["maxCurrentA"]        = board->getMaxCurrentA(); // A
    doc["temp"]               = POWER_MANAGEMENT_MODULE.getChipTempMax();
    doc["vrTemp"]             = POWER_MANAGEMENT_MODULE.getVRTemp();
    doc["vrTempInt"]          = POWER_MANAGEMENT_MODULE.getVRTempInt();
    doc["hashRateTimestamp"]  = history->getCurrentTimestamp();
    // set hashrate values to 0 in shutdown
    doc["hashRate"]           = !shutdown ? SYSTEM_MODULE.getCurrentHashrate() : 0.0;
    doc["hashRate_1m"]        = !shutdown ? history->getCurrentHashrate1m()    : 0.0;
    doc["hashRate_10m"]       = !shutdown ? history->getCurrentHashrate10m()   : 0.0;
    doc["hashRate_1h"]        = !shutdown ? history->getCurrentHashrate1h()    : 0.0;
    doc["hashRate_1d"]        = !shutdown ? history->getCurrentHashrate1d()    : 0.0;
    doc["coreVoltage"]        = board->getAsicVoltageMillis();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["coreVoltageActual"]  = (int) (board->getVout() * 1000.0f);
    doc["fanspeed"]           = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["manualFanSpeed"]     = Config::getFanSpeed();
    doc["fanrpm"]             = POWER_MANAGEMENT_MODULE.getFanRPM(0);
    doc["fanrpm2"]            = (board->getNumFans() > 1) ? POWER_MANAGEMENT_MODULE.getFanRPM(1) : 0;
    doc["fanspeed2"]          = (board->getNumFans() > 1) ? POWER_MANAGEMENT_MODULE.getFanPerc(1) : 0;
    doc["fanCount"]           = board->getNumFans();


    doc["lastpingrtt"]        = get_last_ping_rtt();
    doc["recentpingloss"]     = get_recent_ping_loss();
    doc["shutdown"]           = POWER_MANAGEMENT_MODULE.isShutdown();
    doc["duplicateHWNonces"]  = asic_result_duplicates();

    JsonObject stratum_obj = doc["stratum"].to<JsonObject>();

    // kept for swarm compatibility
    doc["poolDifficulty"]     = STRATUM_MANAGER->getPoolDifficulty();
    doc["networkDifficulty"]  = STRATUM_MANAGER->getNetworkDifficulty();
    doc["foundBlocks"]        = STRATUM_MANAGER->getFoundBlocks();
    doc["totalFoundBlocks"]   = STRATUM_MANAGER->getTotalFoundBlocks();
    doc["sharesAccepted"]     = STRATUM_MANAGER->getSharesAccepted();
    doc["sharesRejected"]     = STRATUM_MANAGER->getSharesRejected();
    doc["bestDiff"]           = STRATUM_MANAGER->getBestDiff();
    doc["bestSessionDiff"]    = STRATUM_MANAGER->getBestSessionDiff();

    // v1 /info: slim legacy shape (external clients like the Blocktrainer terminal)
    STRATUM_MANAGER->getManagerInfoJson(stratum_obj, false);

    // asic temps
    {
        JsonArray arr = doc["asicTemps"].to<JsonArray>();
        for (int i=0;i<board->getAsicCount();i++) {
            arr.add(board->getChipTemp(i));
        }
    }

    // If history was requested, add the history data as a nested object
    if (!shutdown && history_requested) {
        uint64_t span = history_span_ms;
        uint64_t end_timestamp = start_timestamp + span;
        JsonObject json_history = doc["history"].to<JsonObject>();

        HistoryShim *history = SYSTEM_MODULE.getHistory();
        history->exportHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp, history_limit);
    }

    // settings
    PidSettings *pid = board->getPidSettings();
    doc["pidTargetTemp"]      = board->isPIDAvailable() ? pid->targetTemp : -1;
    doc["pidP"]               = (float) pid->p / 100.0f;
    doc["pidI"]               = (float) pid->i / 100.0f;
    doc["pidD"]               = (float) pid->d / 100.0f;

    // Per-channel fan settings (new API; ch0 mirrors existing flat fields for compat)
    {
        JsonArray fans = doc["fans"].to<JsonArray>();
        int numFans = board->getNumFans();
        for (int ch = 0; ch < numFans; ch++) {
            PidSettings* fanPid = board->getPidSettings(ch);
            JsonObject fan = fans.add<JsonObject>();
            fan["label"]        = board->getFanLabel(ch);
            fan["mode"]         = board->getFanMode(ch);
            fan["manualSpeed"]  = Config::getFanManualSpeed(ch);
            fan["overheatTemp"] = Config::getFanOverheatTemp(ch);
            fan["rpm"]          = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
            fan["speedPerc"]    = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
            JsonObject pid_obj  = fan["pid"].to<JsonObject>();
            pid_obj["targetTemp"] = board->isPIDAvailable() ? (int) fanPid->targetTemp : -1;
            pid_obj["p"]          = (float) fanPid->p / 100.0f;
            pid_obj["i"]          = (float) fanPid->i / 100.0f;
            pid_obj["d"]          = (float) fanPid->d / 100.0f;
        }
    }

    doc["hostname"]           = hostname;
    doc["ssid"]               = ssid;
    doc["stratumURL"]         = stratumURL;
    doc["stratumPort"]        = Config::getStratumPortNumber();
    doc["stratumUser"]        = stratumUser;
    doc["stratumEnonceSubscribe"] = Config::isStratumEnonceSubscribe();
    doc["stratumTLS"]         = Config::isStratumTLS();
    doc["fallbackStratumURL"] = fallbackStratumURL;
    doc["fallbackStratumPort"]= Config::getStratumFallbackPortNumber();
    doc["fallbackStratumUser"] = fallbackStratumUser;
    doc["fallbackStratumEnonceSubscribe"] = Config::isStratumFallbackEnonceSubscribe();
    doc["fallbackStratumTLS"] = Config::isStratumFallbackTLS();
    doc["stratumProtocol"]    = Config::getStratumProtocol();
    doc["fallbackStratumProtocol"] = Config::getFallbackStratumProtocol();
    doc["sv2AuthorityPubkey"] = sv2_auth;
    doc["fallbackSv2AuthorityPubkey"] = fb_sv2_auth;
    doc["sv2ChannelType"]     = Config::getSV2ChannelType();
    doc["fallbackSv2ChannelType"] = Config::getFallbackSV2ChannelType();
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["frequency"]          = board->getAsicFrequency();
    doc["defaultFrequency"]   = board->getDefaultAsicFrequency();
    doc["jobInterval"]        = board->getAsicJobIntervalMs();
    doc["stratumDifficulty"] = Config::getStratumDifficulty();
    doc["overheat_temp"]      = Config::getOverheatTemp();
    doc["flipscreen"]         = board->isFlipScreenEnabled() ? 1 : 0;
    doc["invertscreen"]       = Config::isInvertScreenEnabled() ? 1 : 0; // unused?
    doc["autoscreenoff"]      = Config::isAutoScreenOffEnabled() ? 1 : 0;
    doc["invertfanpolarity"]  = board->isInvertFanPolarityEnabled() ? 1 : 0;
    doc["autofanspeed"]       = board->getFanMode(0);
    doc["stratum_keep"]       = Config::isStratumKeepaliveEnabled() ? 1 : 0;
#ifdef VR_FREQUENCY_ENABLED
    doc["vrFrequency"]        = board->getVrFrequency();
    doc["defaultVrFrequency"] = board->getDefaultVrFrequency();
#endif
    doc["otp"]                = Config::isOTPEnabled(); // flag if otp is enabled

    // system screen
    doc["ASICModel"]          = board->getAsicModel();
    doc["uptimeSeconds"]      = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]    = SYSTEM_MODULE.getLastResetReason();
    doc["wifiStatus"]         = SYSTEM_MODULE.getWifiStatus();
    doc["freeHeap"]           = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    doc["freeHeapInt"]        = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    doc["version"]            = esp_app_get_description()->version;
    doc["runningPartition"]   = esp_ota_get_running_partition()->label;

    doc["defaultTheme"]       = board->getDefaultTheme();

    return serialize(doc);
}

extern "C" char *ref_asic_info(const asic_info_t *snap)
{
    shim_set_asic(snap);
    Board *board = SYSTEM_MODULE.getBoard();
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Basisfelder
    doc["ASICModel"]        = board->getAsicModel();
    doc["deviceModel"]      = board->getDeviceModel();
    doc["asicCount"]        = board->getAsicCount();
    doc["defaultFrequency"] = board->getDefaultAsicFrequency();
    doc["defaultVoltage"]   = board->getDefaultAsicVoltageMillis();
    doc["absMaxFrequency"]  = board->getAbsMaxAsicFrequency();
    doc["absMinVoltage"]    = board->getAbsMinAsicVoltageMillis();
    doc["absMaxVoltage"]    = board->getAbsMaxAsicVoltageMillis();
    doc["ecoFrequency"]     = board->getEcoAsicFrequency();
    doc["ecoVoltage"]       = board->getEcoAsicVoltageMillis();

    doc["swarmColor"]       = board->getSwarmColorName();

    // frequencyOptions
    {
        JsonArray arr = doc["frequencyOptions"].to<JsonArray>();
        const auto& freqs = board->getFrequencyOptions();
        for (uint32_t f : freqs) { arr.add(f); }
    }

    // voltageOptions
    {
        JsonArray arr = doc["voltageOptions"].to<JsonArray>();
        const auto& volts = board->getVoltageOptions();
        for (uint32_t v : volts) { arr.add(v); }
    }

    return serialize(doc);
}
