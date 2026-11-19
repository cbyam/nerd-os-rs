/* Host shims for the former GET handler code (c-http-get/*_ref.cpp): the
 * objects and functions it calls, with the original names and return types,
 * returning the values of the differential test's snapshot. */
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ArduinoJson.h"
#include "hashrate_history.h"
#include "http_json.h"
#include "pool_stats.h"
#include "coinbase_decoder.h"

// handler_system.cpp
#define VR_FREQUENCY_ENABLED

struct __attribute__((packed)) PidSettings {
    uint16_t targetTemp;
    uint16_t p;
    uint16_t i;
    uint16_t d;
};

extern const system_info_t *g_snap;
extern const asic_info_t *g_asic;
void shim_set(const system_info_t *s);
void shim_set_asic(const asic_info_t *a);
void shim_set_dash(const dashboard_t *d);
void shim_set_v2sys(const v2_system_t *v);
void shim_set_identify(const v2_identify_t *v);
void shim_set_alert(const alert_info_t *a);
void shim_set_influx(const influx_info_t *i);
void shim_set_settings(const v2_settings_t *s);
void shim_set_can(const can_master_node_t *m);
void safe_free(void *p);

struct OtpShim {
    bool enabled;
    explicit OtpShim(bool e) : enabled(e) {}
    bool isEnabled() { return enabled; }
};

enum wifi_auth_mode_t : int {};
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_simple_t;
#define CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1
char *serialize(JsonDocument &doc);

struct PSRAMAllocator : ArduinoJson::Allocator {
    void *allocate(size_t n) override { return malloc(n); }
    void deallocate(void *p) override { free(p); }
    void *reallocate(void *p, size_t n) override { return realloc(p, n); }
};

#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
size_t heap_caps_get_free_size(int caps);
int64_t esp_timer_get_time(void);
struct esp_app_desc_shim { const char *version; };
const esp_app_desc_shim *esp_app_get_description(void);
struct esp_partition_shim { const char *label; };
const esp_partition_shim *esp_ota_get_running_partition(void);

double get_last_ping_rtt(void);
double get_recent_ping_loss(void);
extern "C" uint64_t asic_result_duplicates(void);

class Asic {
  public:
    uint16_t getSmallCoreCount();
};

class Board {
  public:
    int getAsicCount();
    Asic *getAsics();
    const char *getDeviceModel();
    float getMaxPin();
    float getMinPin();
    float getMaxVin();
    float getMinVin();
    float getMinCurrentA();
    float getMaxCurrentA();
    int getAsicVoltageMillis();
    int getDefaultAsicVoltageMillis();
    virtual float getVout();
    virtual int getNumFans();
    float getChipTemp(int nr);
    virtual bool isPIDAvailable();
    PidSettings *getPidSettings(int ch = 0);
    const char *getFanLabel(int ch) const;
    int getFanMode(int ch);
    int getAsicFrequency();
    int getDefaultAsicFrequency();
    int getAsicJobIntervalMs();
    bool isFlipScreenEnabled();
    bool isInvertFanPolarityEnabled();
    uint32_t getVrFrequency();
    uint32_t getDefaultVrFrequency();
    const char *getAsicModel();
    const char *getDefaultTheme();
    int getAbsMaxAsicFrequency();
    int getAbsMinAsicVoltageMillis();
    int getAbsMaxAsicVoltageMillis();
    int getEcoAsicFrequency();
    int getEcoAsicVoltageMillis();
    const char *getSwarmColorName();
    const std::vector<uint32_t> &getFrequencyOptions();
    const std::vector<uint32_t> &getVoltageOptions();
    virtual bool hasCanExtension();
    virtual ~Board() = default;

  private:
    PidSettings m_pid[2];
    std::vector<uint32_t> m_freqs, m_volts;
};

class HistoryShim {
  public:
    uint64_t getCurrentTimestamp(void);
    double getCurrentHashrate1m();
    double getCurrentHashrate10m();
    double getCurrentHashrate1h();
    double getCurrentHashrate1d();
    void exportHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp,
                           uint64_t current_timestamp, uint32_t limit);
};

class SystemShim {
  public:
    Board *getBoard();
    HistoryShim *getHistory();
    const char *getIPAddress();
    const char *getMacAddress();
    int get_wifi_rssi();
    float getCurrentHashrate();
    int64_t getStartTime();
    const char *getLastResetReason();
    const char *getWifiStatus();
    int getBoardError() const;
};
extern SystemShim SYSTEM_MODULE;

class HashrateMonitorShim {
  public:
    float getChipHashrate(int nr);
    float getChipErrorRate(int nr);
};
extern HashrateMonitorShim HASHRATE_MONITOR;

class NetworkShim {
  public:
    bool isApActive() const;
};
extern NetworkShim NETWORK;

class PowerShim {
  public:
    bool isShutdown();
    float getPower();
    float getCurrent();
    float getChipTempMax();
    float getVRTemp();
    float getVRTempInt();
    float getVoltage();
    uint16_t getFanRPM(int channel);
    uint16_t getFanPerc(int ch = 0);
};
extern PowerShim POWER_MANAGEMENT_MODULE;

enum class StratumProtocol : int {};

struct StratumTaskShim {
    volatile bool m_isConnected = false;
};
struct PingTaskShim {
    double rtt = 0, loss = 0;
    double get_last_ping_rtt() { return rtt; }
    double get_recent_ping_loss() { return loss; }
};
struct StratumConfigShim {
    int protocol = 0;
    bool sv2 = false;
    StratumProtocol getProtocol() { return (StratumProtocol) protocol; }
    bool isSV2() { return sv2; }
    bool isTLS() { return false; }
};
struct PThreadGuard {
    explicit PThreadGuard(int) {}
};
bool pool_policy_pool_diff_err(int policy, int pool);

class StratumManagerShim {
  public:
    uint32_t getPoolDifficulty();
    double getNetworkDifficulty();
    uint32_t getFoundBlocks();
    uint32_t getTotalFoundBlocks();
    uint64_t getSharesAccepted();
    uint64_t getSharesRejected();
    uint64_t getBestDiff();
    uint64_t getBestSessionDiff();
    void getManagerInfoJson(JsonObject &obj, bool verbose);

    int getPoolMode();
    bool isFallback() const;
    bool isDualPool() const;
    bool isUsingFallback();
    int selectedPool();
    pool_stats_pool_t getPoolStats(int pool);
    pool_stats_summary_t getStats();
    const char *getVerifyBlockedReason(int pool) const;
    coinbase_result_t getCoinbaseResult(int pool);
    bool getVerificationOk(int pool);
    uint32_t getVerificationFailCount(int pool);
    uint32_t getVerificationCheckCount(int pool);

    int m_mutex = 0;
    int m_policy = 0;
    StratumTaskShim *m_stratumTasks[2];
    PingTaskShim *m_pingTasks[2];
    StratumConfigShim *m_stratumConfig[2];
};
extern StratumManagerShim *STRATUM_MANAGER;

namespace Config {
char *getWifiSSID();
char *getHostname();
char *getStratumURL();
char *getStratumUser();
char *getStratumFallbackURL();
char *getStratumFallbackUser();
char *getSV2AuthorityPubkey();
char *getFallbackSV2AuthorityPubkey();
uint16_t getFanSpeed();
uint16_t getFanManualSpeed(int ch);
uint16_t getFanOverheatTemp(int ch);
uint16_t getStratumPortNumber();
bool isStratumEnonceSubscribe();
bool isStratumTLS();
uint16_t getStratumFallbackPortNumber();
bool isStratumFallbackEnonceSubscribe();
bool isStratumFallbackTLS();
uint16_t getStratumProtocol();
uint16_t getFallbackStratumProtocol();
uint16_t getSV2ChannelType();
uint16_t getFallbackSV2ChannelType();
uint32_t getStratumDifficulty();
uint16_t getOverheatTemp();
bool isInvertScreenEnabled();
bool isAutoScreenOffEnabled();
bool isStratumKeepaliveEnabled();
bool isOTPEnabled();
uint16_t getPoolMode();
uint16_t getPoolBalance();
uint16_t getCoinbaseVerifyMode(int pool = 0);
uint16_t getCoinbaseMaxFee(int pool = 0);
bool getCoinbaseVerifyForce(int pool = 0);
bool isCanEnabled();
char *getDiscordWebhook();
bool isDiscordWatchdogAlertEnabled();
bool isDiscordBlockFoundAlertEnabled();
bool isDiscordBestDiffAlertEnabled();
bool isDiscordCoinbaseVerifyAlertEnabled();
bool isShowBlockFoundEnabled();
char *getInfluxURL();
char *getInfluxBucket();
char *getInfluxOrg();
char *getInfluxPrefix();
uint16_t getInfluxPort();
bool isInfluxEnabled();
bool isSV2RequireAuth();
bool isFallbackSV2RequireAuth();
bool isMempoolCustom();
char *getMempoolUrl();
bool isFanPidUseMax();
} // namespace Config
