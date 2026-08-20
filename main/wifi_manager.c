#include "wifi_manager.h"

#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "settings_store.h"
#include "wifi_config_store.h"

static const char *TAG = "wifi_manager";

#define BIT_STA_GOT_IP BIT0
#define BIT_STA_ATTEMPT_FAILED BIT1
#define BIT_LOOP_STOP_REQUEST BIT2
#define BIT_LOOP_EXITED BIT3
#define BIT_SCAN_DONE BIT4

#define WIFI_LOOP_STOP_TIMEOUT_MS 8000
#define WIFI_SCAN_TIMEOUT_MS 10000

static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_scan_lock;
static TaskHandle_t s_sta_task;

static volatile bool s_link_up; /* 已拿到 IP */
static volatile bool s_ap_active;
static volatile bool s_connecting;

static wifi_scan_entry_t s_scan_cache[KENKO_WIFI_SCAN_MAX_RESULTS];
static size_t s_scan_cache_count;
static int64_t s_scan_cache_at_us = -1;

const char *wifi_manager_authmode_name(wifi_auth_mode_t authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2-PSK";
    case WIFI_AUTH_ENTERPRISE:
        return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3-PSK";
    case WIFI_AUTH_WAPI_PSK:
        return "WAPI-PSK";
    case WIFI_AUTH_OWE:
        return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:
        return "WPA3-ENT-192";
    default:
        return "UNKNOWN";
    }
}

static void ip4_to_string(const esp_ip4_addr_t *addr, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    if (addr == NULL) {
        strlcpy(out, "0.0.0.0", out_size);
        return;
    }
    esp_ip4addr_ntoa(addr, out, (uint8_t)out_size);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_DISCONNECTED: {
            bool was_up = s_link_up;
            s_link_up = false;
            xEventGroupClearBits(s_events, BIT_STA_GOT_IP);
            xEventGroupSetBits(s_events, BIT_STA_ATTEMPT_FAILED);

            /*
             * 只有“曾经连上过”才算掉线。连接循环里逐个试密码时也会收到这个事件，
             * 那属于一次失败的尝试，不该让状态机走整套断线处理。
             */
            if (was_up) {
                ESP_LOGW(TAG, "wifi link lost");
                app_state_post_event(APP_EVENT_WIFI_LOST);
            } else {
                const wifi_event_sta_disconnected_t *event = event_data;
                ESP_LOGD(TAG, "connect attempt failed reason=%d", event == NULL ? -1 : event->reason);
            }
            break;
        }
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "softAP started");
            break;
        case WIFI_EVENT_AP_STACONNECTED: {
            const wifi_event_ap_staconnected_t *event = event_data;
            if (event != NULL) {
                ESP_LOGI(TAG, "station joined AP aid=%u mac=" MACSTR, event->aid, MAC2STR(event->mac));
            }
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            const wifi_event_ap_stadisconnected_t *event = event_data;
            if (event != NULL) {
                ESP_LOGI(TAG, "station left AP aid=%u mac=" MACSTR, event->aid, MAC2STR(event->mac));
            }
            break;
        }
        case WIFI_EVENT_SCAN_DONE:
            xEventGroupSetBits(s_events, BIT_SCAN_DONE);
            break;
        default:
            break;
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_link_up = true;
        xEventGroupClearBits(s_events, BIT_STA_ATTEMPT_FAILED);
        xEventGroupSetBits(s_events, BIT_STA_GOT_IP);
        ESP_LOGI(TAG, "wifi connected, got IP");
        app_state_post_event(APP_EVENT_WIFI_CONNECTED);
    }
}

/** SoftAP 的静态地址只需要在 netif 创建后配置一次。 */
static esp_err_t configure_softap_network(void)
{
    esp_netif_ip_info_t ip_info = {0};
    IP4_ADDR(&ip_info.ip, KENKO_AP_IP0, KENKO_AP_IP1, KENKO_AP_IP2, KENKO_AP_IP3);
    IP4_ADDR(&ip_info.gw, KENKO_AP_IP0, KENKO_AP_IP1, KENKO_AP_IP2, KENKO_AP_IP3);
    IP4_ADDR(&ip_info.netmask, KENKO_AP_NETMASK0, KENKO_AP_NETMASK1, KENKO_AP_NETMASK2, KENKO_AP_NETMASK3);

    ESP_RETURN_ON_ERROR(esp_netif_dhcps_stop(s_ap_netif), TAG, "stop dhcps failed");
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(s_ap_netif, &ip_info), TAG, "set AP ip failed");

    /* DHCP 下发的 DNS 指向设备自身，captive portal 才能接管域名解析。 */
    esp_netif_dns_info_t dns = {
        .ip.u_addr.ip4 = ip_info.ip,
        .ip.type = ESP_IPADDR_TYPE_V4,
    };
    ESP_RETURN_ON_ERROR(esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns), TAG,
                        "set AP dns failed");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(s_ap_netif), TAG, "start dhcps failed");
    return ESP_OK;
}

static bool stop_requested(void)
{
    return (xEventGroupGetBits(s_events) & BIT_LOOP_STOP_REQUEST) != 0;
}

/** 可被停止请求打断的等待。 */
static bool interruptible_delay(uint32_t delay_ms)
{
    EventBits_t bits =
        xEventGroupWaitBits(s_events, BIT_LOOP_STOP_REQUEST, pdFALSE, pdFALSE, pdMS_TO_TICKS(delay_ms));
    return (bits & BIT_LOOP_STOP_REQUEST) == 0;
}

static bool try_connect(const wifi_credential_t *credential)
{
    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, credential->ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, credential->password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set sta config failed: %s", esp_err_to_name(err));
        return false;
    }

    xEventGroupClearBits(s_events, BIT_STA_GOT_IP | BIT_STA_ATTEMPT_FAILED);

    err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "connect failed: %s", esp_err_to_name(err));
        return false;
    }

    EventBits_t bits =
        xEventGroupWaitBits(s_events, BIT_STA_GOT_IP | BIT_STA_ATTEMPT_FAILED | BIT_LOOP_STOP_REQUEST,
                            pdFALSE, pdFALSE, pdMS_TO_TICKS(KENKO_WIFI_CONNECT_TIMEOUT_MS));

    if ((bits & BIT_STA_GOT_IP) != 0) {
        return true;
    }

    esp_wifi_disconnect();
    return false;
}

static void sta_connect_task(void *arg)
{
    (void)arg;
    s_connecting = true;

    while (!stop_requested()) {
        wifi_credential_list_t list;
        wifi_config_store_load(&list);

        if (list.count == 0) {
            ESP_LOGW(TAG, "no wifi configs saved, switching to provisioning");
            app_state_post_event(APP_EVENT_ENTER_PROVISIONING);
            break;
        }

        bool connected = false;
        for (size_t index = 0; index < list.count && !stop_requested(); ++index) {
            ESP_LOGI(TAG, "trying wifi[%u/%u] ssid=%s", (unsigned)(index + 1), (unsigned)list.count,
                     list.items[index].ssid);
            if (try_connect(&list.items[index])) {
                ESP_LOGI(TAG, "connected to %s", list.items[index].ssid);
                connected = true;
                break;
            }
        }

        if (connected || stop_requested()) {
            break;
        }

        ESP_LOGW(TAG, "all %u wifi configs failed, retrying in %u ms", (unsigned)list.count,
                 (unsigned)KENKO_WIFI_RETRY_BACKOFF_MS);
        if (!interruptible_delay(KENKO_WIFI_RETRY_BACKOFF_MS)) {
            break;
        }
    }

    s_connecting = false;
    s_sta_task = NULL;
    xEventGroupSetBits(s_events, BIT_LOOP_EXITED);
    vTaskDelete(NULL);
}

esp_err_t wifi_manager_init(void)
{
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events != NULL, ESP_ERR_NO_MEM, TAG, "event group alloc failed");

    s_scan_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_scan_lock != NULL, ESP_ERR_NO_MEM, TAG, "scan mutex alloc failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop create failed");

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_sta_netif != NULL && s_ap_netif != NULL, ESP_FAIL, TAG, "netif create failed");

    app_settings_t settings;
    settings_store_get(&settings);
    ESP_RETURN_ON_ERROR(esp_netif_set_hostname(s_sta_netif, settings.device_name), TAG,
                        "set hostname failed");

    ESP_RETURN_ON_ERROR(configure_softap_network(), TAG, "configure AP network failed");

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL),
        TAG, "wifi handler register failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL),
        TAG, "ip handler register failed");

    /* 凭据由 LittleFS 管理，不需要 WiFi 驱动再往 NVS 里写一份。 */
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi ram storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set initial STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");
    return ESP_OK;
}

esp_err_t wifi_manager_start_sta_loop(void)
{
    if (s_sta_task != NULL) {
        return ESP_OK;
    }

    xEventGroupClearBits(s_events, BIT_LOOP_STOP_REQUEST | BIT_LOOP_EXITED);
    BaseType_t created = xTaskCreate(sta_connect_task, "wifi_sta_loop", KENKO_TASK_STACK_WIFI, NULL,
                                     KENKO_TASK_PRIORITY_WIFI, &s_sta_task);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "sta loop task create failed");
    return ESP_OK;
}

void wifi_manager_stop_sta_loop(void)
{
    if (s_sta_task == NULL) {
        return;
    }

    /*
     * 用停止标志让任务自己走完退出路径。强制 vTaskDelete 有可能在任务持有
     * WiFi 驱动内部锁时把它杀掉，那个锁再也拿不回来。
     */
    xEventGroupSetBits(s_events, BIT_LOOP_STOP_REQUEST);
    EventBits_t bits = xEventGroupWaitBits(s_events, BIT_LOOP_EXITED, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_LOOP_STOP_TIMEOUT_MS));
    if ((bits & BIT_LOOP_EXITED) == 0) {
        ESP_LOGE(TAG, "sta loop did not exit within %d ms", WIFI_LOOP_STOP_TIMEOUT_MS);
    }
    xEventGroupClearBits(s_events, BIT_LOOP_STOP_REQUEST);
}

esp_err_t wifi_manager_start_provisioning_ap(void)
{
    wifi_manager_stop_sta_loop();
    esp_wifi_disconnect();
    s_link_up = false;

    app_settings_t settings;
    settings_store_get(&settings);

    wifi_config_t config = {0};
    strlcpy((char *)config.ap.ssid, settings.device_name, sizeof(config.ap.ssid));
    config.ap.ssid_len = (uint8_t)strnlen((const char *)config.ap.ssid, sizeof(config.ap.ssid));
    config.ap.channel = KENKO_AP_CHANNEL;
    config.ap.max_connection = KENKO_AP_MAX_CONNECTION;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.pmf_cfg.required = false;

    /* APSTA 而不是纯 AP：扫描周边热点需要 STA 接口在线。 */
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set APSTA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "set AP config failed");

    s_ap_active = true;
    ESP_LOGI(TAG, "provisioning AP started ssid=%s ip=%s", config.ap.ssid, KENKO_AP_IP_ADDR);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_provisioning_ap(void)
{
    if (!s_ap_active) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "restore STA mode failed");
    s_ap_active = false;
    ESP_LOGI(TAG, "provisioning AP stopped");
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_link_up;
}

void wifi_manager_get_status(wifi_status_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->sta_connected = s_link_up;
    out->ap_active = s_ap_active;
    out->connecting = s_connecting && !s_link_up;
    strlcpy(out->ip, "0.0.0.0", sizeof(out->ip));
    strlcpy(out->netmask, "0.0.0.0", sizeof(out->netmask));
    strlcpy(out->gateway, "0.0.0.0", sizeof(out->gateway));
    strlcpy(out->ap_ip, KENKO_AP_IP_ADDR, sizeof(out->ap_ip));

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        mode = WIFI_MODE_NULL;
    }
    switch (mode) {
    case WIFI_MODE_STA:
        out->mode = "sta";
        break;
    case WIFI_MODE_AP:
        out->mode = "ap";
        break;
    case WIFI_MODE_APSTA:
        out->mode = "apsta";
        break;
    default:
        out->mode = "off";
        break;
    }

    if (s_link_up) {
        wifi_ap_record_t record = {0};
        if (esp_wifi_sta_get_ap_info(&record) == ESP_OK) {
            strlcpy(out->ssid, (const char *)record.ssid, sizeof(out->ssid));
            out->rssi = record.rssi;
            out->channel = record.primary;
        }

        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
            ip4_to_string(&ip_info.ip, out->ip, sizeof(out->ip));
            ip4_to_string(&ip_info.netmask, out->netmask, sizeof(out->netmask));
            ip4_to_string(&ip_info.gw, out->gateway, sizeof(out->gateway));
        }
    }

    if (s_ap_active) {
        wifi_sta_list_t sta_list = {0};
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            out->ap_clients = (uint8_t)sta_list.num;
        }
    }
}

static int compare_scan_entry(const void *left, const void *right)
{
    const wifi_scan_entry_t *a = left;
    const wifi_scan_entry_t *b = right;
    return b->rssi - a->rssi;
}

/** 同一个 SSID 可能有多个 AP，只保留信号最强的一个。 */
static void collect_unique(const wifi_ap_record_t *records, uint16_t record_count)
{
    s_scan_cache_count = 0;

    for (uint16_t index = 0; index < record_count; ++index) {
        const char *ssid = (const char *)records[index].ssid;
        if (ssid[0] == '\0') {
            continue;
        }

        bool merged = false;
        for (size_t existing = 0; existing < s_scan_cache_count; ++existing) {
            if (strcmp(s_scan_cache[existing].ssid, ssid) != 0) {
                continue;
            }
            if (records[index].rssi > s_scan_cache[existing].rssi) {
                s_scan_cache[existing].rssi = records[index].rssi;
                s_scan_cache[existing].channel = records[index].primary;
                s_scan_cache[existing].authmode = records[index].authmode;
            }
            merged = true;
            break;
        }
        if (merged || s_scan_cache_count >= KENKO_WIFI_SCAN_MAX_RESULTS) {
            continue;
        }

        wifi_scan_entry_t *entry = &s_scan_cache[s_scan_cache_count++];
        strlcpy(entry->ssid, ssid, sizeof(entry->ssid));
        entry->rssi = records[index].rssi;
        entry->channel = records[index].primary;
        entry->authmode = records[index].authmode;
    }

    qsort(s_scan_cache, s_scan_cache_count, sizeof(s_scan_cache[0]), compare_scan_entry);
}

static esp_err_t refresh_scan_cache(void)
{
    const wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    xEventGroupClearBits(s_events, BIT_SCAN_DONE);

    /* 异步扫描：同步扫描会把 HTTP 任务卡住好几秒。 */
    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan start failed: %s", esp_err_to_name(err));
        return err;
    }

    EventBits_t bits =
        xEventGroupWaitBits(s_events, BIT_SCAN_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT_MS));
    if ((bits & BIT_SCAN_DONE) == 0) {
        esp_wifi_scan_stop();
        esp_wifi_clear_ap_list();
        ESP_LOGW(TAG, "scan timed out");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t ap_count = 0;
    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        esp_wifi_clear_ap_list();
        return err;
    }
    if (ap_count == 0) {
        s_scan_cache_count = 0;
        s_scan_cache_at_us = esp_timer_get_time();
        return ESP_OK;
    }

    if (ap_count > KENKO_WIFI_SCAN_MAX_RESULTS) {
        ap_count = KENKO_WIFI_SCAN_MAX_RESULTS;
    }

    wifi_ap_record_t *records = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        esp_wifi_clear_ap_list();
        return ESP_ERR_NO_MEM;
    }

    uint16_t record_count = ap_count;
    err = esp_wifi_scan_get_ap_records(&record_count, records);
    if (err == ESP_OK) {
        collect_unique(records, record_count);
        s_scan_cache_at_us = esp_timer_get_time();
    }
    free(records);
    return err;
}

esp_err_t wifi_manager_scan(wifi_scan_entry_t *out, size_t max, size_t *count, bool force)
{
    if (out == NULL || count == NULL || max == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;

    if (xSemaphoreTake(s_scan_lock, pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT_MS + 2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ESP_OK;
    int64_t now = esp_timer_get_time();
    bool cache_valid =
        s_scan_cache_at_us >= 0 && (now - s_scan_cache_at_us) < (int64_t)KENKO_WIFI_SCAN_CACHE_TTL_MS * 1000;

    if (force || !cache_valid) {
        err = refresh_scan_cache();
    }

    if (err == ESP_OK || s_scan_cache_at_us >= 0) {
        size_t copied = s_scan_cache_count < max ? s_scan_cache_count : max;
        memcpy(out, s_scan_cache, copied * sizeof(wifi_scan_entry_t));
        *count = copied;
        err = ESP_OK;
    }

    xSemaphoreGive(s_scan_lock);
    return err;
}
