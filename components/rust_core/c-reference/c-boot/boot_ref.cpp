/* The former app_main() of main/main.cpp (verbatim from bfe7bc59, with
 * newStratumManager()) on recording shims; vTaskSuspend() ends the run. */
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#define Q1373 1
#define CONFIG_AUTO_FAN_SPEED_VALUE 1

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERROR_CHECK(x) ((void) (x))
typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
               ESP_RST_TASK_WDT, ESP_RST_WDT } esp_reset_reason_t;
#define MALLOC_CAP_INTERNAL 1
#define pdMS_TO_TICKS(ms) (ms)
#define NULL_TASK nullptr
static const char *esp_err_to_name(esp_err_t) { return ""; }

static std::string g_trace;
static void tr(const std::string &s) { g_trace += s; g_trace += ';'; }
#define ESP_LOGI(tag, ...) ((void) (tag))
#define ESP_LOGE(tag, ...) ((void) (tag))
#define ESP_LOGW(tag, fmt, ...) tr(std::string("logw:") + fmt)

static std::jmp_buf g_end;

/* injected inputs */
struct In {
    int reset_reason;
    bool self_test;
    uint64_t best_diff;
    bool can_slave;
    int pool_mode;
    bool has_user;
    bool can_enabled;
    bool can_ext;
    bool hr_counter;
    int shutdown_after; /* idle iterations until isShutdown() */
    size_t heap[8];     /* free heap per idle iteration */
};
static In g_in;
static int g_idle;

static void initWatchdog() { tr("early_init"); }
static void mbedtls_platform_set_calloc_free(void *(*)(size_t, size_t), void (*)(void *)) {}
static void *psram_calloc(size_t, size_t) { return nullptr; }
static void free_psram(void *) {}
static esp_err_t nvs_flash_init() { return ESP_OK; }
static esp_err_t gpio_install_isr_service(int) { return ESP_OK; }
static esp_err_t esp_netif_init() { return ESP_OK; }
static esp_err_t esp_event_loop_create_default() { return ESP_OK; }
static size_t esp_psram_get_size() { return 8 << 20; }
static void vTaskDelay(int ms) { tr("delay:" + std::to_string(ms)); if (ms == 10000) g_idle++; }
static void vTaskSuspend(void *) { tr("suspend"); std::longjmp(g_end, 1); }
static size_t heap_caps_get_free_size(int) { return g_in.heap[g_idle % 8]; }
typedef void (*TaskFn)(void *);
static void xTaskCreatePSRAM(TaskFn, const char *name, int, void *, int, void *) { tr(std::string("task:") + name); }
static void xTaskCreate(TaskFn, const char *name, int, void *, int, void *) { tr(std::string("task:") + name); }

struct Asic {};
struct Board {
    void loadSettings() { tr("board_load_settings"); }
    bool initBoard() { tr("board_init"); return true; }
    void detectEthernet() { tr("board_detect_eth"); }
    const char *getDeviceModel() { return "Q1373"; }
    int getVersion() { return 1; }
    bool selfTest() { tr("self_test"); return true; }
    bool isCanSlave() { return g_in.can_slave; }
    int getCanTxPin() { return 21; }
    int getCanRxPin() { return 16; }
    bool initAsics() { tr("init_asics"); return true; }
    bool hasHashrateCounter() { return g_in.hr_counter; }
    Asic *getAsics() { return nullptr; }
    bool hasCanExtension() { return g_in.can_ext; }
    bool hasEthernet() { return false; }
};
struct Q1373B : Board { Q1373B() { tr("board_create"); } };
static void initEthernet(Board *) { tr("eth_init"); }

struct SystemShim {
    esp_reset_reason_t showLastResetReason() { return (esp_reset_reason_t) g_in.reset_reason; }
    void setBoard(Board *) { tr("system_set_board"); }
    void init() { tr("system_init"); }
    void initDisplay() {}
    static void taskWrapper(void *) {}
    void setStartupDone() { tr("startup_done"); }
};
static SystemShim SYSTEM_MODULE;
struct PowerShim {
    static void taskWrapper(void *) {}
    void lock() {}
    void unlock() {}
    bool isShutdown() { return g_idle >= g_in.shutdown_after; }
};
static PowerShim POWER_MANAGEMENT_MODULE;
struct HashrateShim { void start(Board *, Asic *) { tr("hashrate"); } };
static HashrateShim HASHRATE_MONITOR;
struct StratumManager {
    enum class PoolMode { FAILOVER, DUAL };
    explicit StratumManager(PoolMode m) { tr(std::string("stratum_manager:") + (m == PoolMode::DUAL ? "dual" : "failover")); }
    void loadSettings() { tr("stratum_load_settings"); }
    static void taskWrapper(void *) {}
};
static StratumManager *STRATUM_MANAGER = nullptr;
struct NetShim { void shutdownApOnce() { tr("ap_off_once"); } };
static NetShim NETWORK;
struct Discord {
    void start() { tr("discord_start"); }
    void sendWatchdogAlert() { tr("watchdog_alert"); }
};
static Discord discordAlerter;
struct Otp { bool init() { tr("otp_init"); return true; } };
static Otp otp;
struct Sntp { void start() { tr("sntp_start"); } };
static Sntp sntp;
struct Apis { static void taskWrapper(void *) {} };
static Apis APIs_FETCHER;
struct Ota { static void taskWrapper(void *) {} };
static Ota FACTORY_OTA_UPDATER;
static void setup_network(bool) { tr("setup_network"); }
static void create_jobs_task(void *) {}
static void ASIC_result_task(void *) {}
static void influx_task(void *) {}
static void wifi_monitor_task(void *) {}
static void can_master_task(void *) {}
static void can_slave_task(void *) {}
static void can_slave_result_task(void *) {}
static void can_slave_telemetry_task(void *) {}
static void can_init(int, int) { tr("can_init"); }
#define NVS_CONFIG_STRATUM_USER "stratumuser"
namespace Config {
static uint64_t getBestDiff() { return g_in.best_diff; }
static bool isSelfTestEnabled() { return g_in.self_test; }
static uint16_t getPoolMode() { return (uint16_t) g_in.pool_mode; }
static char *cfgGetStrAlloc(const char *, const char *) { return strdup(g_in.has_user ? "user" : ""); }
static bool isCanEnabled() { return g_in.can_enabled; }
static void migrate_config() {}
} // namespace Config
struct MemoryGuard {
    char *p;
    explicit MemoryGuard(char *p) : p(p) {}
    ~MemoryGuard() { free(p); }
};
static const char *TAG = "nerd*axe";

StratumManager* newStratumManager() {
    int mode = (int) Config::getPoolMode();
    switch (mode) {
    case 0:
        return new StratumManager(StratumManager::PoolMode::FAILOVER);
    case 1:
        return new StratumManager(StratumManager::PoolMode::DUAL);
    default:
        ESP_LOGE(TAG, "invalid pool mode %d", (int) mode);
        return nullptr;
    }
}

static void old_app_main(void)
{
    initWatchdog();

    // use PSRAM because TLS costs a lot of internal RAM
    mbedtls_platform_set_calloc_free(psram_calloc, free_psram);

    ESP_LOGI(TAG, "Welcome to the Nerd*Axe - hack the planet!");
    ESP_ERROR_CHECK(nvs_flash_init());

    // shows and saves last reset reason
    esp_reset_reason_t reason = SYSTEM_MODULE.showLastResetReason();

    // migrate config
    Config::migrate_config();

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(esp_netif_init());

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

#ifdef NERDQAXEPLUS
    Board *board = new NerdQaxePlus();
#endif
#ifdef NERDQAXEPLUS2
    Board *board = new NerdQaxePlus2();
#endif
#ifdef NERDOCTAXEPLUS
    Board *board = new NerdOctaxePlus();
#endif
#ifdef NERDAXE
    Board *board = new NerdAxe();
#endif
#ifdef NERDOCTAXEGAMMA
    Board *board = new NerdOctaxeGamma();
#endif
#ifdef NERDAXEGAMMA
    Board *board = new NerdaxeGamma();
#endif
#ifdef NERDAXEGAIA
    Board *board = new NerdaxeGaia();
#endif
#ifdef NERDHAXEGAMMA
    Board *board = new NerdHaxeGamma();
#endif
#ifdef NERDEKO
    Board *board = new NerdEko();
#endif
#ifdef NERDQX
    Board *board = new NerdQX();
#endif
#ifdef Q1370
    Board *board = new Q1370B();
#endif
#ifdef Q1373
    Board *board = new Q1373B();
#endif

    // initialize everything non-asic-specific like
    // fan and serial and load settings from nvs
    // on-board W5500 (Q1370/Q1373) or HAT detected in the constructor (NerdAxeGaia)
    initEthernet(board);

    board->loadSettings();
    board->initBoard();

    // pluggable W5500 HATs that can only be detected once the board is up (NerdQAxe
    // family: I2C and the temp mux on shared pins are known). Still before the display init,
    // the W5500 has to be set up before the LCD.
    board->detectEthernet();
    initEthernet(board);


    SYSTEM_MODULE.setBoard(board);

    size_t total_psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM found with %dMB", total_psram / (1024 * 1024));
    ESP_LOGI(TAG, "Found Device Model: %s", board->getDeviceModel());
    ESP_LOGI(TAG, "Found Board Version: %d", board->getVersion());

    uint64_t best_diff = Config::getBestDiff();
    bool should_self_test = Config::isSelfTestEnabled();
    if (should_self_test && !best_diff) {
        board->selfTest();
        vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
    }

    const bool canSlave = board->isCanSlave();

    if (canSlave) {
        // ----------------------------------------------------------------
        // CAN Slave mode: no network, no stratum, no HTTP server.
        // Receives raw ASIC jobs from master via CAN, forwards to ASICs,
        // returns nonces via CAN.
        // ----------------------------------------------------------------
        ESP_LOGI(TAG, "CAN Slave mode");

        SYSTEM_MODULE.init();
        SYSTEM_MODULE.initDisplay();

        xTaskCreatePSRAM(SYSTEM_MODULE.taskWrapper, "SYSTEM_task", 4096, &SYSTEM_MODULE, 3, NULL);
        xTaskCreatePSRAM(POWER_MANAGEMENT_MODULE.taskWrapper, "power mangement", 8192, (void *) &POWER_MANAGEMENT_MODULE, 10, NULL);
        SYSTEM_MODULE.setStartupDone();

        can_init(board->getCanTxPin(), board->getCanRxPin());

        POWER_MANAGEMENT_MODULE.lock();
        if (!board->initAsics()) {
            ESP_LOGE(TAG, "error initializing board %s", board->getDeviceModel());
        }
        POWER_MANAGEMENT_MODULE.unlock();

        xTaskCreatePSRAM(can_slave_task, "can slave", 4096, NULL, 10, NULL);
        xTaskCreatePSRAM(can_slave_result_task, "can result", 4096, NULL, 10, NULL);
        xTaskCreatePSRAM(can_slave_telemetry_task, "can telem", 4096, NULL, 5, NULL);

        if (board->hasHashrateCounter()) {
            HASHRATE_MONITOR.start(board, board->getAsics());
        }

    } else {
        // ----------------------------------------------------------------
        // Master mode: full stack — network, stratum, HTTP, CAN sender.
        // ----------------------------------------------------------------
        STRATUM_MANAGER = newStratumManager();

        if (!STRATUM_MANAGER) {
            ESP_LOGE(TAG, "stratumManager is null! main task suspended.");
            vTaskSuspend(NULL);
        }

        STRATUM_MANAGER->loadSettings();

        SYSTEM_MODULE.init();
        SYSTEM_MODULE.initDisplay();

        // Start display-driving tasks BEFORE setup_network() so the display can show
        // the Portal screen when there is no network. setup_network() blocks forever
        // when neither WiFi nor LAN is available, so the SYSTEM_task must be running
        // beforehand — otherwise portalScreen() is never called and the display stays
        // on a blank (white) screen.
        xTaskCreatePSRAM(SYSTEM_MODULE.taskWrapper, "SYSTEM_task", 4096, &SYSTEM_MODULE, 3, NULL);
        xTaskCreatePSRAM(POWER_MANAGEMENT_MODULE.taskWrapper, "power mangement", 8192, (void *) &POWER_MANAGEMENT_MODULE, 10, NULL);
        SYSTEM_MODULE.setStartupDone();

        setup_network(board->hasEthernet());

        // when a username is configured we will continue with startup and start mining
        char *username = Config::cfgGetStrAlloc(NVS_CONFIG_STRATUM_USER, "");
        MemoryGuard gUsername(username);
        if (username && username[0] != '\0') {
            // wifi is connected, switch the AP off (no-op if already done by NetworkManager)
            NETWORK.shutdownApOnce();

            // start the discord alerter early
            discordAlerter.start();

            // initialize OTP
            if (!otp.init()) {
                ESP_LOGE(TAG, "error init otp");
            }

            // start SNTP
            sntp.start();

            // we only use alerting if we are in a normal operating mode
            if (reason == ESP_RST_TASK_WDT) {
                discordAlerter.sendWatchdogAlert();
            }

            // and continue with initialization
            POWER_MANAGEMENT_MODULE.lock();
            if (!board->initAsics()) {
                ESP_LOGE(TAG, "error initializing board %s", board->getDeviceModel());
            }
            POWER_MANAGEMENT_MODULE.unlock();

            xTaskCreatePSRAM(create_jobs_task, "stratum miner", 8192, NULL, 10, NULL);
            xTaskCreatePSRAM(ASIC_result_task, "asic result", 8192, NULL, 15, NULL);
            xTaskCreatePSRAM(influx_task, "influx", 8192, NULL, 1, NULL);
            xTaskCreatePSRAM(APIs_FETCHER.taskWrapper, "apis ticker", 8192, (void *) &APIs_FETCHER, 5, NULL);
            xTaskCreatePSRAM(wifi_monitor_task, "wifi monitor", 4096, NULL, 1, NULL);
            // only with CAN hardware: on the NerdQAxe family the CAN pins are shared
            // with the ethernet HAT
            if (Config::isCanEnabled() && board->hasCanExtension()) {
                can_init(board->getCanTxPin(), board->getCanRxPin());
                xTaskCreate(can_master_task, "can master", 4096, NULL, 5, NULL);
            }
            // OTA writes SPI flash which disables cache — must NOT run on PSRAM stack
            xTaskCreate(FACTORY_OTA_UPDATER.taskWrapper, "ota updater", 8192, (void *) &FACTORY_OTA_UPDATER, 1, NULL);
            xTaskCreatePSRAM(StratumManager::taskWrapper, "stratum manager", 8192, (void *) STRATUM_MANAGER, 5, NULL);

            if (board->hasHashrateCounter()) {
                HASHRATE_MONITOR.start(board, board->getAsics());
            }
        }
    }

    // char* taskList = (char*) malloc(8192);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            // not needed, we deregister the WDT in the stratumtask on shutdown
            //esp_task_wdt_deinit();
            ESP_LOGW(TAG, "suspended");
            vTaskSuspend(NULL);
        }

        size_t free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        if (free_internal_heap < 10000) {
            ESP_LOGW(TAG, "*** WARNING *** Free internal heap: %d bytes", free_internal_heap);
        }
        // monitor_all_task_watermarks();
    }
}


extern "C" size_t ref_boot(const In *in, char *buf, size_t len)
{
    g_in = *in;
    g_idle = 0;
    g_trace.clear();
    if (!setjmp(g_end)) {
        old_app_main();
    }
    snprintf(buf, len, "%s", g_trace.c_str());
    return g_trace.size();
}
