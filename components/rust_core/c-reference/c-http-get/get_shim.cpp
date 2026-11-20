/* The shim objects of get_shim.h on the differential test's snapshot. */
#include "get_shim.h"

const system_info_t *g_snap;
const asic_info_t *g_asic;

namespace {
Board s_board;
Asic s_asic;
HistoryShim s_history;
StratumManagerShim s_manager;
StratumTaskShim s_tasks[2];
PingTaskShim s_pings[2];
StratumConfigShim s_configs[2];
esp_app_desc_shim s_app;
esp_partition_shim s_part;
const system_info_t &S() { return *g_snap; }
char *dup(const char *s) { return s ? strdup(s) : nullptr; }
} // namespace

SystemShim SYSTEM_MODULE;
PowerShim POWER_MANAGEMENT_MODULE;
StratumManagerShim *STRATUM_MANAGER = &s_manager;

void shim_set(const system_info_t *s)
{
    g_snap = s;
    g_asic = nullptr;
    for (int i = 0; i < 2; i++) {
        const manager_pool_t &p = s->manager.pools[i];
        s_tasks[i].m_isConnected = p.connected;
        s_pings[i].rtt = p.ping_rtt;
        s_pings[i].loss = p.ping_loss;
        s_configs[i].protocol = p.protocol;
        s_configs[i].sv2 = p.encrypted;
        s_manager.m_stratumTasks[i] = &s_tasks[i];
        s_manager.m_pingTasks[i] = &s_pings[i];
        s_manager.m_stratumConfig[i] = &s_configs[i];
    }
    s_app.version = s->version;
    s_part.label = s->running_partition;
}

void shim_set_asic(const asic_info_t *a) { g_asic = a; }

char *serialize(JsonDocument &doc)
{
    std::string out;
    serializeJson(doc, out);
    return strdup(out.c_str());
}

size_t heap_caps_get_free_size(int caps) { return caps == MALLOC_CAP_SPIRAM ? S().free_heap : S().free_heap_int; }
int64_t esp_timer_get_time(void) { return (int64_t) S().now_ms * 1000; }
const esp_app_desc_shim *esp_app_get_description(void) { return &s_app; }
const esp_partition_shim *esp_ota_get_running_partition(void) { return &s_part; }
double get_last_ping_rtt(void) { return S().last_ping_rtt; }
double get_recent_ping_loss(void) { return S().recent_ping_loss; }
extern "C" uint64_t asic_result_duplicates(void) { return S().duplicate_hw_nonces; }

uint16_t Asic::getSmallCoreCount() { return (uint16_t) S().small_core_count; }

int Board::getAsicCount() { return g_asic ? g_asic->asic_count : S().asic_count; }
Asic *Board::getAsics() { return S().small_core_count ? &s_asic : nullptr; }
const char *Board::getDeviceModel() { return g_asic ? g_asic->device_model : S().device_model; }
float Board::getMaxPin() { return S().max_power; }
float Board::getMinPin() { return S().min_power; }
float Board::getMaxVin() { return S().max_voltage; }
float Board::getMinVin() { return S().min_voltage; }
float Board::getMinCurrentA() { return S().min_current_a; }
float Board::getMaxCurrentA() { return S().max_current_a; }
int Board::getAsicVoltageMillis() { return S().core_voltage; }
int Board::getDefaultAsicVoltageMillis() { return g_asic ? g_asic->default_voltage : S().default_core_voltage; }
float Board::getVout() { return S().vout; }
int Board::getNumFans() { return S().fan_count; }
float Board::getChipTemp(int nr) { return S().asic_temps[nr]; }
bool Board::isPIDAvailable() { return S().pid_available; }
PidSettings *Board::getPidSettings(int ch)
{
    const fan_info_t &f = S().fans[ch];
    m_pid[ch] = {f.target_temp, f.p, f.i, f.d};
    if (ch == 0) {
        m_pid[0] = {S().pid_target_temp, S().pid_p, S().pid_i, S().pid_d};
    }
    return &m_pid[ch];
}
const char *Board::getFanLabel(int ch) const { return S().fans[ch].label; }
int Board::getFanMode(int ch) { return S().fans[ch].mode; }
int Board::getAsicFrequency() { return S().frequency; }
int Board::getDefaultAsicFrequency() { return g_asic ? g_asic->default_frequency : S().default_frequency; }
int Board::getAsicJobIntervalMs() { return S().job_interval; }
bool Board::isFlipScreenEnabled() { return S().flip_screen; }
bool Board::isInvertFanPolarityEnabled() { return S().invert_fan_polarity; }
uint32_t Board::getVrFrequency() { return S().vr_frequency; }
uint32_t Board::getDefaultVrFrequency() { return S().default_vr_frequency; }
const char *Board::getAsicModel() { return g_asic ? g_asic->asic_model : S().asic_model; }
const char *Board::getDefaultTheme() { return S().default_theme; }
int Board::getAbsMaxAsicFrequency() { return g_asic->abs_max_frequency; }
int Board::getAbsMinAsicVoltageMillis() { return g_asic->abs_min_voltage; }
int Board::getAbsMaxAsicVoltageMillis() { return g_asic->abs_max_voltage; }
int Board::getEcoAsicFrequency() { return g_asic->eco_frequency; }
int Board::getEcoAsicVoltageMillis() { return g_asic->eco_voltage; }
const char *Board::getSwarmColorName() { return g_asic->swarm_color; }
const std::vector<uint32_t> &Board::getFrequencyOptions()
{
    m_freqs.assign(g_asic->frequency_options, g_asic->frequency_options + g_asic->frequency_count);
    return m_freqs;
}
const std::vector<uint32_t> &Board::getVoltageOptions()
{
    m_volts.assign(g_asic->voltage_options, g_asic->voltage_options + g_asic->voltage_count);
    return m_volts;
}

uint64_t HistoryShim::getCurrentTimestamp(void) { return S().hashrate_timestamp; }
double HistoryShim::getCurrentHashrate1m() { return S().hashrate_1m; }
double HistoryShim::getCurrentHashrate10m() { return S().hashrate_10m; }
double HistoryShim::getCurrentHashrate1h() { return S().hashrate_1h; }
double HistoryShim::getCurrentHashrate1d() { return S().hashrate_1d; }

Board *SystemShim::getBoard() { return &s_board; }
HistoryShim *SystemShim::getHistory() { return &s_history; }
const char *SystemShim::getIPAddress() { return S().hostip; }
const char *SystemShim::getMacAddress() { return S().mac_addr; }
int SystemShim::get_wifi_rssi() { return S().wifi_rssi; }
float SystemShim::getCurrentHashrate() { return S().hashrate; }
int64_t SystemShim::getStartTime() { return esp_timer_get_time() - S().uptime_seconds * 1000000; }
const char *SystemShim::getLastResetReason() { return S().last_reset_reason; }
const char *SystemShim::getWifiStatus() { return S().wifi_status; }

bool PowerShim::isShutdown() { return S().shutdown; }
float PowerShim::getPower() { return S().power; }
float PowerShim::getCurrent() { return S().current; }
float PowerShim::getChipTempMax() { return S().temp; }
float PowerShim::getVRTemp() { return S().vr_temp; }
float PowerShim::getVRTempInt() { return S().vr_temp_int; }
float PowerShim::getVoltage() { return S().voltage; }
uint16_t PowerShim::getFanRPM(int channel) { return S().fans[channel].rpm; }
uint16_t PowerShim::getFanPerc(int ch) { return S().fans[ch].speed_perc; }

bool pool_policy_pool_diff_err(int, int pool) { return S().manager.pools[pool].pool_diff_err; }

uint32_t StratumManagerShim::getPoolDifficulty() { return S().pool_difficulty; }
double StratumManagerShim::getNetworkDifficulty() { return S().network_difficulty; }
uint32_t StratumManagerShim::getFoundBlocks() { return S().found_blocks; }
uint32_t StratumManagerShim::getTotalFoundBlocks() { return S().total_found_blocks; }
uint64_t StratumManagerShim::getSharesAccepted() { return S().shares_accepted; }
uint64_t StratumManagerShim::getSharesRejected() { return S().shares_rejected; }
uint64_t StratumManagerShim::getBestDiff() { return S().manager.total_best_diff; }
uint64_t StratumManagerShim::getBestSessionDiff() { return S().best_session_diff; }
int StratumManagerShim::getPoolMode() { return S().manager.pool_mode; }
bool StratumManagerShim::isFallback() const { return S().manager.fallback; }
bool StratumManagerShim::isDualPool() const { return S().manager.dual; }
bool StratumManagerShim::isUsingFallback() { return S().manager.using_fallback; }
int StratumManagerShim::selectedPool() { return S().manager.selected; }
pool_stats_pool_t StratumManagerShim::getPoolStats(int pool)
{
    const manager_pool_t &p = S().manager.pools[pool];
    pool_stats_pool_t st = {};
    st.accepted = p.accepted;
    st.rejected = p.rejected;
    st.best_session = p.best_session;
    st.network_difficulty = p.network_difficulty;
    st.pool_difficulty = p.pool_difficulty;
    return st;
}
pool_stats_summary_t StratumManagerShim::getStats()
{
    const manager_info_t &m = S().manager;
    pool_stats_summary_t sum = {};
    sum.accepted = m.accepted;
    sum.rejected = m.rejected;
    sum.best_session = m.best_session;
    sum.network_difficulty = m.network_difficulty;
    sum.pool_difficulty = m.pool_difficulty;
    return sum;
}
const char *StratumManagerShim::getVerifyBlockedReason(int pool) const { return S().manager.pools[pool].verify_blocked; }

namespace Config {
char *getWifiSSID() { return dup(S().ssid); }
char *getHostname() { return dup(S().hostname); }
char *getStratumURL() { return dup(S().stratum_url); }
char *getStratumUser() { return dup(S().stratum_user); }
char *getStratumFallbackURL() { return dup(S().fallback_stratum_url); }
char *getStratumFallbackUser() { return dup(S().fallback_stratum_user); }
char *getSV2AuthorityPubkey() { return dup(S().sv2_authority_pubkey); }
char *getFallbackSV2AuthorityPubkey() { return dup(S().fallback_sv2_authority_pubkey); }
uint16_t getFanSpeed() { return S().manual_fan_speed; }
uint16_t getFanManualSpeed(int ch) { return S().fans[ch].manual_speed; }
uint16_t getFanOverheatTemp(int ch) { return S().fans[ch].overheat_temp; }
uint16_t getStratumPortNumber() { return S().stratum_port; }
bool isStratumEnonceSubscribe() { return S().stratum_enonce_subscribe; }
bool isStratumTLS() { return S().stratum_tls; }
uint16_t getStratumFallbackPortNumber() { return S().fallback_stratum_port; }
bool isStratumFallbackEnonceSubscribe() { return S().fallback_stratum_enonce_subscribe; }
bool isStratumFallbackTLS() { return S().fallback_stratum_tls; }
uint16_t getStratumProtocol() { return S().stratum_protocol; }
uint16_t getFallbackStratumProtocol() { return S().fallback_stratum_protocol; }
uint16_t getSV2ChannelType() { return S().sv2_channel_type; }
uint16_t getFallbackSV2ChannelType() { return S().fallback_sv2_channel_type; }
uint32_t getStratumDifficulty() { return S().stratum_difficulty; }
uint16_t getOverheatTemp() { return S().overheat_temp; }
bool isInvertScreenEnabled() { return S().invert_screen; }
bool isAutoScreenOffEnabled() { return S().auto_screen_off; }
bool isStratumKeepaliveEnabled() { return S().stratum_keep; }
bool isOTPEnabled() { return S().otp; }
uint16_t getPoolMode() { return S().manager.config_pool_mode; }
uint16_t getPoolBalance() { return S().manager.pool_balance; }
} // namespace Config

extern "C" size_t ref_http_json_sizeof(int which)
{
    switch (which) {
    case 0: return sizeof(manager_info_t);
    case 1: return sizeof(fan_info_t);
    case 2: return sizeof(system_info_t);
    case 3: return sizeof(asic_info_t);
    case 4: return sizeof(dashboard_t);
    case 5: return sizeof(v2_system_t);
    case 6: return sizeof(v2_identify_t);
    case 7: return sizeof(alert_info_t);
    case 8: return sizeof(influx_info_t);
    case 9: return sizeof(wifi_network_t);
    case 10: return sizeof(settings_pool_t);
    case 11: return sizeof(v2_settings_t);
    default: return sizeof(can_master_node_t);
    }
}

// the v2 endpoints: their snapshots mapped onto the system snapshot, the
// fields only they have in these
namespace {
system_info_t s_mapped;
asic_info_t s_asic_mapped;
const dashboard_t *g_dash;
const alert_info_t *g_alert;
const influx_info_t *g_influx;
const v2_settings_t *g_settings;
bool s_ap_active, s_can_enabled, s_has_can;
int s_board_error;
uint16_t s_cb_mode[2], s_cb_fee[2];
bool s_cb_force[2];
} // namespace
HashrateMonitorShim HASHRATE_MONITOR;
NetworkShim NETWORK;

void shim_set_dash(const dashboard_t *d)
{
    system_info_t s = {};
    s.uptime_seconds = d->uptime_seconds;
    s.shutdown = d->shutdown;
    s.overheat_temp = d->overheat_temp;
    s.hashrate_timestamp = d->hashrate_timestamp;
    s.hashrate = d->hashrate;
    s.hashrate_1m = d->hashrate_1m;
    s.hashrate_10m = d->hashrate_10m;
    s.hashrate_1h = d->hashrate_1h;
    s.hashrate_1d = d->hashrate_1d;
    s.best_session_diff = d->best_session_diff;
    s.shares_accepted = d->shares_accepted;
    s.shares_rejected = d->shares_rejected;
    s.frequency = d->frequency;
    s.asic_count = d->asic_count;
    s.small_core_count = d->small_core_count;
    s.asic_temps = d->asic_temps;
    s.power = d->watts;
    s.min_power = d->min_power;
    s.max_power = d->max_power;
    s.voltage = d->voltage;
    s.min_voltage = d->min_voltage;
    s.max_voltage = d->max_voltage;
    s.current = d->current;
    s.min_current_a = d->min_current_a;
    s.max_current_a = d->max_current_a;
    s.vout = d->vout;
    s.temp = d->asic_temp;
    s.vr_temp = d->vr_temp;
    s.vr_temp_int = d->vr_temp_int;
    s.fan_count = d->fan_count;
    for (int ch = 0; ch < 2; ch++) {
        s.fans[ch].speed_perc = d->fan_speed[ch];
        s.fans[ch].rpm = d->fan_rpm[ch];
    }
    s.fans[1].overheat_temp = d->vr_overheat_temp;
    s.manager = d->manager;
    s.stratum_url = d->pool_host[0];
    s.fallback_stratum_url = d->pool_host[1];
    s.stratum_port = (uint16_t) d->pool_port[0];
    s.fallback_stratum_port = (uint16_t) d->pool_port[1];
    s.stratum_user = d->pool_user[0];
    s.fallback_stratum_user = d->pool_user[1];
    s.now_ms = d->now_ms;
    s_mapped = s;
    shim_set(&s_mapped);
    g_dash = d;
    s_board_error = d->board_error;
    s_has_can = d->has_can_extension;
    s_can_enabled = d->can_enabled;
    for (int p = 0; p < 2; p++) {
        s_cb_mode[p] = d->coinbase[p].verify_mode;
        s_cb_fee[p] = d->coinbase[p].max_fee;
        s_cb_force[p] = d->coinbase[p].force;
    }
}

void shim_set_v2sys(const v2_system_t *v)
{
    system_info_t s = {};
    s.device_model = v->device_model;
    s.asic_model = v->asic_model;
    s.version = v->version;
    s.uptime_seconds = v->uptime_seconds;
    s.last_reset_reason = v->last_reset_reason;
    s.hostname = v->hostname;
    s.ssid = v->ssid;
    s.mac_addr = v->mac_addr;
    s.hostip = v->ip_addr;
    s.wifi_status = v->wifi_status;
    s.wifi_rssi = v->wifi_rssi;
    s.free_heap = v->free_heap;
    s.free_heap_int = v->free_heap_int;
    s_mapped = s;
    shim_set(&s_mapped);
}

void shim_set_identify(const v2_identify_t *v)
{
    system_info_t s = {};
    s.device_model = v->device_model;
    s.default_theme = v->default_theme;
    s.otp = v->otp;
    s_mapped = s;
    shim_set(&s_mapped);
    s_ap_active = v->ap_active;
    s_can_enabled = v->can_enabled;
}

bool Board::hasCanExtension() { return s_has_can; }
int SystemShim::getBoardError() const { return s_board_error; }
float HashrateMonitorShim::getChipHashrate(int nr) { return g_dash->chip_hashrate[nr]; }
float HashrateMonitorShim::getChipErrorRate(int nr) { return g_dash->chip_error_rate[nr]; }
bool NetworkShim::isApActive() const { return s_ap_active; }

coinbase_result_t StratumManagerShim::getCoinbaseResult(int pool)
{
    const dash_coinbase_t &c = g_dash->coinbase[pool];
    coinbase_result_t r = {};
    r.network_difficulty = c.network_difficulty;
    r.block_height = c.block_height;
    strncpy(r.scriptsig, c.scriptsig, sizeof(r.scriptsig) - 1);
    r.total_value_satoshis = c.total_value_satoshis;
    r.user_value_satoshis = c.user_value_satoshis;
    return r;
}
bool StratumManagerShim::getVerificationOk(int pool) { return g_dash->coinbase[pool].verification_ok; }
uint32_t StratumManagerShim::getVerificationFailCount(int pool) { return g_dash->coinbase[pool].fail_count; }
uint32_t StratumManagerShim::getVerificationCheckCount(int pool) { return g_dash->coinbase[pool].check_count; }

namespace Config {
uint16_t getCoinbaseVerifyMode(int pool) { return s_cb_mode[pool]; }
uint16_t getCoinbaseMaxFee(int pool) { return s_cb_fee[pool]; }
bool getCoinbaseVerifyForce(int pool) { return s_cb_force[pool]; }
bool isCanEnabled() { return s_can_enabled; }
char *getDiscordWebhook() { return g_alert->has_webhook ? strdup("https://example.invalid/hook") : (g_alert->watchdog ? nullptr : strdup("")); }
bool isDiscordWatchdogAlertEnabled() { return g_alert->watchdog; }
bool isDiscordBlockFoundAlertEnabled() { return g_alert->block_found; }
bool isDiscordBestDiffAlertEnabled() { return g_alert->best_diff; }
bool isDiscordCoinbaseVerifyAlertEnabled() { return g_alert->coinbase_verify; }
bool isShowBlockFoundEnabled() { return g_alert->show_block_found_screen; }
char *getInfluxURL() { return dup(g_influx->url); }
char *getInfluxBucket() { return dup(g_influx->bucket); }
char *getInfluxOrg() { return dup(g_influx->org); }
char *getInfluxPrefix() { return dup(g_influx->prefix); }
uint16_t getInfluxPort() { return g_influx->port; }
bool isInfluxEnabled() { return g_influx->enabled; }
bool isSV2RequireAuth() { return g_settings->pools[0].sv2_require_auth; }
bool isFallbackSV2RequireAuth() { return g_settings->pools[1].sv2_require_auth; }
bool isMempoolCustom() { return g_settings->mempool_custom; }
char *getMempoolUrl() { return dup(g_settings->mempool_url); }
bool isFanPidUseMax() { return g_settings->pid_use_max; }
} // namespace Config

void safe_free(void *p) { free(p); }

void shim_set_alert(const alert_info_t *a) { g_alert = a; }
void shim_set_influx(const influx_info_t *i) { g_influx = i; }

void shim_set_settings(const v2_settings_t *v)
{
    system_info_t s = {};
    s.asic_model = v->asic_model;
    s.device_model = v->device_model;
    s.version = v->version;
    s.otp = v->otp;
    s.frequency = v->frequency;
    s.core_voltage = v->core_voltage;
    s.vr_frequency = v->vr_frequency;
    s.default_vr_frequency = v->default_vr_frequency;
    s.manager.config_pool_mode = v->pool_mode;
    s.manager.pool_balance = v->pool_balance;
    s.stratum_keep = v->stratum_keep;
    s.job_interval = v->job_interval;
    s.stratum_difficulty = v->stratum_difficulty;
    const settings_pool_t &p0 = v->pools[0], &p1 = v->pools[1];
    s.stratum_url = p0.url;
    s.stratum_port = p0.port;
    s.stratum_user = p0.user;
    s.stratum_enonce_subscribe = p0.enonce_subscribe;
    s.stratum_tls = p0.tls;
    s.stratum_protocol = p0.protocol;
    s.sv2_authority_pubkey = p0.sv2_authority_pubkey;
    s.sv2_channel_type = p0.sv2_channel_type;
    s.fallback_stratum_url = p1.url;
    s.fallback_stratum_port = p1.port;
    s.fallback_stratum_user = p1.user;
    s.fallback_stratum_enonce_subscribe = p1.enonce_subscribe;
    s.fallback_stratum_tls = p1.tls;
    s.fallback_stratum_protocol = p1.protocol;
    s.fallback_sv2_authority_pubkey = p1.sv2_authority_pubkey;
    s.fallback_sv2_channel_type = p1.sv2_channel_type;
    s.pid_available = v->pid_available;
    s.fan_count = v->fan_count;
    s.fans[0] = v->fans[0];
    s.fans[1] = v->fans[1];
    s.pid_target_temp = v->fans[0].target_temp;
    s.pid_p = v->fans[0].p;
    s.pid_i = v->fans[0].i;
    s.pid_d = v->fans[0].d;
    s.invert_fan_polarity = v->invert_fan_polarity;
    s.hostname = v->hostname;
    s.ssid = v->ssid;
    s.flip_screen = v->flip_screen;
    s.invert_screen = v->invert_screen;
    s.auto_screen_off = v->auto_screen_off;
    s_mapped = s;
    shim_set(&s_mapped);

    asic_info_t a = {};
    a.asic_model = v->asic_model;
    a.device_model = v->device_model;
    a.default_frequency = v->default_frequency;
    a.default_voltage = v->default_core_voltage;
    a.abs_min_voltage = v->abs_min_core_voltage;
    a.abs_max_voltage = v->abs_max_core_voltage;
    a.eco_frequency = v->eco_frequency;
    a.eco_voltage = v->eco_core_voltage;
    a.frequency_options = v->frequency_options;
    a.frequency_count = v->frequency_count;
    a.voltage_options = v->voltage_options;
    a.voltage_count = v->voltage_count;
    s_asic_mapped = a;
    g_asic = &s_asic_mapped;

    g_settings = v;
    s_ap_active = v->ap_active;
    s_has_can = v->has_can_extension;
    s_can_enabled = v->can_enabled;
    for (int p = 0; p < 2; p++) {
        s_cb_mode[p] = v->pools[p].coinbase_verify_mode;
        s_cb_fee[p] = v->pools[p].coinbase_max_fee;
        s_cb_force[p] = v->pools[p].coinbase_verify_force;
    }
}

void shim_set_can(const can_master_node_t *m)
{
    system_info_t s = {};
    s.mac_addr = m->mac;
    s.device_model = m->device_model;
    s.version = m->version;
    s.shutdown = m->shutdown;
    s.hashrate = m->hashrate;
    s.power = m->power;
    s.temp = m->temp;
    s.vr_temp = m->vr_temp;
    s.current = m->current;
    s.vout = m->vout;
    s.free_heap_int = (uint64_t) m->free_heap_int;
    s.frequency = m->frequency;
    s.core_voltage = m->core_voltage;
    s.flip_screen = m->flip_screen;
    s.auto_screen_off = m->auto_screen_off;
    s.asic_count = m->asic_count;
    s.asic_temps = m->asic_temps;
    s.fan_count = m->fan_count;
    s.fans[0] = m->fans[0];
    s.fans[1] = m->fans[1];
    s.pid_target_temp = m->fans[0].target_temp;
    s.pid_p = m->fans[0].p;
    s.pid_i = m->fans[0].i;
    s.pid_d = m->fans[0].d;
    s_mapped = s;
    shim_set(&s_mapped);
    s_board_error = m->board_error;
}
