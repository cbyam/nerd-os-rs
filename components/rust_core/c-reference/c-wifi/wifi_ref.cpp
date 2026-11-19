/* The former WiFi event handler of main/network/connect.cpp (verbatim from
 * af853db6, statics through event_handler) on recording shims. */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

typedef int esp_err_t;
#define ESP_OK 0
typedef const char *esp_event_base_t;
static const char WIFI_EVENT_S[] = "WIFI", IP_EVENT_S[] = "IP";
#define WIFI_EVENT WIFI_EVENT_S
#define IP_EVENT IP_EVENT_S
enum { WIFI_EVENT_STA_START = 2, WIFI_EVENT_STA_DISCONNECTED = 5, WIFI_EVENT_SCAN_DONE = 1 };
enum { IP_EVENT_STA_GOT_IP = 0 };
struct esp_ip4_addr_t { uint32_t addr; };
struct esp_netif_ip_info_t { esp_ip4_addr_t ip; };
struct ip_event_got_ip_t { esp_netif_ip_info_t ip_info; };
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ipaddr) (int) (((ipaddr)->addr >> 0) & 0xff), (int) (((ipaddr)->addr >> 8) & 0xff), \
                       (int) (((ipaddr)->addr >> 16) & 0xff), (int) (((ipaddr)->addr >> 24) & 0xff)
#define EXT_RAM_BSS_ATTR
#define ESP_LOGI(tag, ...) ((void) (tag))
#define ESP_LOGW(tag, ...) ((void) (tag))
#define ESP_LOGE(tag, ...) ((void) (tag))
#define pdMS_TO_TICKS(ms) (ms)
#define BIT0 1
#define BIT1 2
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_SCAN_MAX_APS 20
struct wifi_ap_record_t { uint8_t ssid[33]; int8_t rssi; int authmode; };
typedef struct EventGroup *EventGroupHandle_t;
typedef struct Netif esp_netif_t;
typedef void (*WifiHookFn)();
typedef enum { WIFI_CONNECTED, WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_DISCONNECTING, WIFI_CONNECT_FAILED,
               WIFI_RETRYING, WIFI_PAUSED } wifi_status_t;

static std::string g_trace;
static void tr(const char *s) { g_trace += s; g_trace += ';'; }
static void xEventGroupClearBits(EventGroupHandle_t, int b) { tr(b == WIFI_CONNECTED_BIT ? "clr_conn" : "clr_fail"); }
static void xEventGroupSetBits(EventGroupHandle_t, int b) { tr(b == WIFI_CONNECTED_BIT ? "set_conn" : "set_fail"); }
static void vTaskDelay(int) { tr("backoff"); }
static esp_err_t esp_wifi_connect() { tr("connect"); return ESP_OK; }
static esp_err_t esp_wifi_scan_get_ap_records(uint16_t *, wifi_ap_record_t *) { return ESP_OK; }
static void MINER_set_wifi_status(wifi_status_t st, uint16_t n)
{
    char b[32];
    snprintf(b, sizeof(b), "status%d/%u", (int) st, (unsigned) n);
    tr(b);
}
void MINER_set_ap_status(bool) {}
static void hook_got_ip() { tr("hook_ip"); }
static void hook_disc() { tr("hook_disc"); }

static const char *TAG = "wifi";

#define WIFI_MAXIMUM_RETRY 5

static EventGroupHandle_t s_wifi_event_group = nullptr;

static int s_retry_num = 0;

static char s_ip_addr[20] = "0.0.0.0";
static char s_mac_addr[18] = {0};
static bool s_ip_valid = false;
static bool s_has_ssid = false;

/* WiFi scan state */
static volatile bool s_is_scanning = false;
static volatile bool s_scan_suppress_reconnect = false;
static volatile bool s_paused = false;
static uint16_t s_scan_ap_count = 0;
static EXT_RAM_BSS_ATTR wifi_ap_record_t s_scan_ap_info[WIFI_SCAN_MAX_APS];

static esp_netif_t *s_netif_ap = nullptr;
static esp_netif_t *s_netif_sta = nullptr;

static bool s_inited = false;

/* Optional hooks used by NetworkManager */
static WifiHookFn s_hook_got_ip = nullptr;
static WifiHookFn s_hook_disconnected = nullptr;

void wifi_pause(bool paused)
{
    s_paused = paused;
    s_retry_num = 0;
    if (paused) {
        MINER_set_wifi_status(WIFI_PAUSED, 0);
    }
}

bool connect_get_ip_addr(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return s_ip_valid;
    }
    /* Always NUL-terminate */
    strncpy(buf, s_ip_addr, buf_len);
    buf[buf_len - 1] = '\0';
    return s_ip_valid;
}

const char *connect_get_mac_addr(void)
{
    return s_mac_addr;
}

esp_netif_t *wifi_get_sta_netif(void)
{
    return s_netif_sta;
}
esp_netif_t *wifi_get_ap_netif(void)
{
    return s_netif_ap;
}

void wifi_set_hook_got_ip(WifiHookFn fn)
{
    s_hook_got_ip = fn;
}
void wifi_set_hook_disconnected(WifiHookFn fn)
{
    s_hook_disconnected = fn;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void) arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_has_ssid) {
            esp_wifi_connect();
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        s_scan_ap_count = WIFI_SCAN_MAX_APS;
        if (esp_wifi_scan_get_ap_records(&s_scan_ap_count, s_scan_ap_info) != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records failed");
            s_scan_ap_count = 0;
        }
        ESP_LOGI(TAG, "WiFi scan done: %u APs", (unsigned) s_scan_ap_count);
        s_is_scanning = false;
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* Clear connected state */
        s_ip_valid = false;
        strncpy(s_ip_addr, "0.0.0.0", sizeof(s_ip_addr));
        s_ip_addr[sizeof(s_ip_addr) - 1] = '\0';

        if (s_wifi_event_group) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        if (s_hook_disconnected) {
            s_hook_disconnected();
        }

        /* No SSID configured, scan in progress or WiFi paused — don't reconnect */
        if (!s_has_ssid || s_scan_suppress_reconnect || s_paused) {
            return;
        }

        /* Small backoff */
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (s_paused) {
            return;
        }

        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection...");
            MINER_set_wifi_status(WIFI_RETRYING, s_retry_num);
        } else {
            /* Signal initial failure so wifi_connect() can return */
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGW(TAG, "Could not connect to WiFi (initial).");
            MINER_set_wifi_status(WIFI_CONNECT_FAILED, 0);

            /* Keep reconnecting indefinitely in background */
            s_retry_num = 0;
            esp_wifi_connect();
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        s_ip_valid = true;

        ESP_LOGI(TAG, "WiFi got IP: %s", s_ip_addr);

        s_retry_num = 0;

        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        MINER_set_wifi_status(WIFI_CONNECTED, 0);

        if (s_hook_got_ip) {
            s_hook_got_ip();
        }
        return;
    }
}


static EventGroupHandle_t s_eg_dummy = (EventGroupHandle_t) 1;

extern "C" void ref_wifi_reset(bool has_ssid)
{
    s_wifi_event_group = s_eg_dummy;
    s_retry_num = 0;
    strcpy(s_ip_addr, "0.0.0.0");
    s_ip_valid = false;
    s_has_ssid = has_ssid;
    s_is_scanning = false;
    s_scan_suppress_reconnect = false;
    s_paused = false;
    s_hook_got_ip = hook_got_ip;
    s_hook_disconnected = hook_disc;
    g_trace.clear();
}

/* ev: 0 start, 1 disconnected, 2 got ip (ip), 3 pause(true), 4 pause(false),
 * 5 scan suppress on (scan start), 6 scan suppress off + reconnect (scan end) */
extern "C" void ref_wifi_event(int ev, uint32_t ip)
{
    switch (ev) {
    case 0: event_handler(nullptr, WIFI_EVENT, WIFI_EVENT_STA_START, nullptr); break;
    case 1: event_handler(nullptr, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, nullptr); break;
    case 2: {
        ip_event_got_ip_t e = {{{ip}}};
        event_handler(nullptr, IP_EVENT, IP_EVENT_STA_GOT_IP, &e);
        break;
    }
    case 3: wifi_pause(true); break;
    case 4: wifi_pause(false); break;
    case 5: s_scan_suppress_reconnect = true; break;
    case 6:
        // the end of wifi_scan()
        s_scan_suppress_reconnect = false;
        if (s_has_ssid) {
            esp_wifi_connect();
        }
        break;
    }
}

extern "C" size_t ref_wifi_trace(char *buf, size_t len)
{
    snprintf(buf, len, "%s", g_trace.c_str());
    g_trace.clear();
    return strlen(buf);
}

extern "C" bool ref_wifi_ip(char *buf, size_t len) { return connect_get_ip_addr(buf, len); }
