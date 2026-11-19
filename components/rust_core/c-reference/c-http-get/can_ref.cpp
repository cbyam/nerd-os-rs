/* The former document building of GET /api/v2/can/swarm
 * (main/http_server/handler_can_swarm.cpp), verbatim (extracted by a script
 * from the commit before HTTP step C3d), on the shims of get_shim.h for the
 * master board and the real (Rust) CAN master for the slaves. */
#include "get_shim.h"

#define CAN_SLAVE_MAX 32

// main/tasks/can_sender.h
// Live telemetry pushed by each slave ~1/s. Settings removed — see can_slave_config_t.
typedef struct __attribute__((__packed__)) {
    uint8_t  version;           // CAN_TELEMETRY_VERSION
    float    temp;              // max ASIC chip temp (°C)
    float    vrTemp;            // VReg temp (°C)
    float    asicTemps[4];      // per-chip temps (°C)
    uint16_t fanRpm;            // fan 0 RPM
    uint16_t fanRpm2;           // fan 1 RPM
    uint8_t  fanSpeed;          // fan 0 duty %
    uint8_t  fanSpeed2;         // fan 1 duty %
    float    power;             // Watt
    uint16_t current;           // mA
    uint16_t coreVoltageActual; // mV
    float    hashRate;          // GH/s (from HASHRATE_MONITOR)
    uint8_t  shutdown;          // 1 = explicit manual shutdown active
    uint8_t  boardError;        // Board::Error enum value (0 = NONE)
    uint32_t freeHeap;          // free internal RAM (bytes)
} can_slave_telemetry_t;        // 49 bytes → 7 CAN frames

// Device info + settings, sent once after ASSIGN and after any CMD_SET_*.
// Also re-sent on CMD_GET_CONFIG request.
typedef struct __attribute__((__packed__)) {
    char     deviceModel[16];   // board->getDeviceModel()
    char     fwVersion[16];     // esp_app_get_description()->version
    uint16_t freqMhz;           // ASIC frequency (MHz)
    uint16_t voltageMv;         // ASIC voltage (mV)
    uint8_t  fan0Mode;          // fan ch0 mode (0=manual,2=pid,3=linked)
    uint8_t  fan0Speed;         // fan ch0 manual duty %
    uint8_t  fan0TargetTemp;    // fan ch0 PID target °C
    uint8_t  fan0Overheat;      // fan ch0 shutdown temp °C
    uint8_t  fan1Mode;          // fan ch1 mode
    uint8_t  fan1Speed;         // fan ch1 manual duty %
    uint8_t  fan1TargetTemp;    // fan ch1 target °C
    uint8_t  fan1Overheat;      // fan ch1 shutdown temp °C
    uint8_t  flipScreen;        // 1 = display flipped
    uint8_t  autoScreenOff;     // 1 = auto screen off enabled
} can_slave_config_t;           // 46 bytes → 7 CAN frames

// main/tasks/can_master_task.h (implemented in Rust)
extern "C" {
bool can_master_is_slave_active(uint8_t slave_id);
bool can_master_is_slave_known(uint8_t slave_id);
bool can_master_is_slave_foreign(uint8_t slave_id);
bool can_master_get_slave_mac(uint8_t slave_id, uint8_t out[6]);
bool can_master_get_slave_telemetry(uint8_t slave_id, can_slave_telemetry_t *out);
bool can_master_get_slave_config(uint8_t slave_id, can_slave_config_t *out);
float can_master_get_slave_fleet_power(void);
}

extern "C" char *ref_can_swarm(const can_master_node_t *snap)
{
    shim_set_can(snap);
    Board *board       = SYSTEM_MODULE.getBoard();
    bool   shutdown    = POWER_MANAGEMENT_MODULE.isShutdown();
    int    numFans     = board->getNumFans();

    float masterHashRate = !shutdown ? SYSTEM_MODULE.getCurrentHashrate() : 0.0f;
    float masterPower    = POWER_MANAGEMENT_MODULE.getPower();

    // Fleet totals: master + all active slaves
    float slaveHashRate = 0.0f;
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;
        can_slave_telemetry_t st = {};
        if (can_master_get_slave_telemetry((uint8_t) i, &st) && !st.shutdown) {
            slaveHashRate += st.hashRate;
        }
    }
    float fleetHashRate = masterHashRate + slaveHashRate;

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- Fleet totals ---
    JsonObject fleet = doc["fleet"].to<JsonObject>();
    fleet["hashRate"] = fleetHashRate;
    fleet["power"]    = masterPower + can_master_get_slave_fleet_power();

    JsonArray arr = doc["nodes"].to<JsonArray>();

    // --- Node [0]: master board ---
    {
        JsonObject master = arr.add<JsonObject>();
        master["id"]       = 0;
        master["mac"]      = SYSTEM_MODULE.getMacAddress();
        master["active"]   = true;
        master["foreign"]  = false;
        master["isMaster"] = true;

        master["deviceModel"]       = board->getDeviceModel();
        master["version"]           = esp_app_get_description()->version;
        master["hashRate"]          = masterHashRate;
        master["temp"]              = POWER_MANAGEMENT_MODULE.getChipTempMax();
        master["vrTemp"]            = POWER_MANAGEMENT_MODULE.getVRTemp();
        master["power"]             = POWER_MANAGEMENT_MODULE.getPower();
        master["current"]           = (int) POWER_MANAGEMENT_MODULE.getCurrent();
        master["coreVoltageActual"] = (int) (board->getVout() * 1000.0f);
        master["fanRpm"]            = POWER_MANAGEMENT_MODULE.getFanRPM(0);
        master["fanRpm2"]           = numFans > 1 ? POWER_MANAGEMENT_MODULE.getFanRPM(1) : 0;
        master["fanSpeed"]          = POWER_MANAGEMENT_MODULE.getFanPerc(0);
        master["fanSpeed2"]         = numFans > 1 ? POWER_MANAGEMENT_MODULE.getFanPerc(1) : 0;
        master["shutdown"]          = shutdown;
        master["boardError"]        = (int) SYSTEM_MODULE.getBoardError();
        master["freeHeapInt"]       = (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        master["frequency"]         = board->getAsicFrequency();
        master["coreVoltage"]       = board->getAsicVoltageMillis();
        master["flipScreen"]        = board->isFlipScreenEnabled();
        master["autoScreenOff"]     = Config::isAutoScreenOffEnabled();

        {
            JsonArray asicTemps = master["asicTemps"].to<JsonArray>();
            for (int j = 0; j < board->getAsicCount(); j++) {
                asicTemps.add(board->getChipTemp(j));
            }
        }

        {
            JsonArray fans = master["fans"].to<JsonArray>();
            for (int ch = 0; ch < numFans; ch++) {
                JsonObject fan   = fans.add<JsonObject>();
                fan["mode"]        = board->getFanMode(ch);
                fan["manualSpeed"] = Config::getFanManualSpeed(ch);
                fan["overheatTemp"]= Config::getFanOverheatTemp(ch);
                fan["targetTemp"]  = board->getPidSettings(ch)->targetTemp;
                fan["rpm"]         = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
                fan["speedPerc"]   = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
            }
        }
    }

    // --- Nodes [1..N]: CAN slaves ---
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;

        uint8_t mac[6];
        can_master_get_slave_mac((uint8_t) i, mac);

        can_slave_telemetry_t t   = {};
        can_slave_config_t    cfg = {};
        bool has_telem  = can_master_get_slave_telemetry((uint8_t) i, &t);
        bool has_config = can_master_get_slave_config((uint8_t) i, &cfg);

        char device_model[sizeof(cfg.deviceModel) + 1] = {};
        char fw_version[sizeof(cfg.fwVersion)     + 1] = {};
        if (has_config) {
            memcpy(device_model, cfg.deviceModel, sizeof(cfg.deviceModel));
            memcpy(fw_version,   cfg.fwVersion,   sizeof(cfg.fwVersion));
        }

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        JsonObject node = arr.add<JsonObject>();
        node["id"]      = i;
        node["mac"]     = mac_str;
        node["active"]  = can_master_is_slave_active((uint8_t) i);
        node["foreign"] = can_master_is_slave_foreign((uint8_t) i);

        node["deviceModel"]        = device_model;
        node["version"]            = fw_version;
        node["hashRate"]           = has_telem ? t.hashRate          : 0.0f;
        node["temp"]               = has_telem ? t.temp              : 0.0f;
        node["vrTemp"]             = has_telem ? t.vrTemp            : 0.0f;
        node["power"]              = has_telem ? t.power             : 0.0f;
        node["current"]            = has_telem ? t.current           : 0;
        node["coreVoltageActual"]  = has_telem ? t.coreVoltageActual : 0;
        node["fanRpm"]             = has_telem ? t.fanRpm            : 0;
        node["fanRpm2"]            = has_telem ? t.fanRpm2           : 0;
        node["fanSpeed"]           = has_telem ? t.fanSpeed          : 0;
        node["fanSpeed2"]          = has_telem ? t.fanSpeed2         : 0;
        node["shutdown"]           = has_telem && t.shutdown;
        node["boardError"]         = has_telem ? t.boardError        : 0;
        node["freeHeapInt"]        = has_telem ? t.freeHeap          : 0;
        node["frequency"]          = has_config ? cfg.freqMhz        : 0;
        node["coreVoltage"]        = has_config ? cfg.voltageMv      : 0;
        node["flipScreen"]         = has_config && cfg.flipScreen;
        node["autoScreenOff"]      = has_config && cfg.autoScreenOff;

        {
            JsonArray asicTemps = node["asicTemps"].to<JsonArray>();
            for (int j = 0; j < 4; j++) {
                asicTemps.add(has_telem ? t.asicTemps[j] : 0.0f);
            }
        }

        {
            JsonArray fans = node["fans"].to<JsonArray>();
            for (int ch = 0; ch < 2; ch++) {
                JsonObject fan      = fans.add<JsonObject>();
                fan["mode"]         = has_config ? (ch == 0 ? cfg.fan0Mode       : cfg.fan1Mode)       : 0;
                fan["manualSpeed"]  = has_config ? (ch == 0 ? cfg.fan0Speed      : cfg.fan1Speed)      : 0;
                fan["overheatTemp"] = has_config ? (ch == 0 ? cfg.fan0Overheat   : cfg.fan1Overheat)   : 0;
                fan["targetTemp"]   = has_config ? (ch == 0 ? cfg.fan0TargetTemp : cfg.fan1TargetTemp) : 0;
                fan["rpm"]          = has_telem  ? (ch == 0 ? t.fanRpm           : t.fanRpm2)          : 0;
                fan["speedPerc"]    = has_telem  ? (ch == 0 ? t.fanSpeed         : t.fanSpeed2)        : 0;
            }
        }
    }

    return serialize(doc);
}
