#include "wifi_manager.h"

#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "device_info.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "status_led.h"
#include "wifi_config_store.h"

static const char *TAG = "wifi_manager";
static const int WIFI_CONNECTED_BIT = BIT0;

static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static EventGroupHandle_t s_wifi_events;
static TaskHandle_t s_sta_task;
static bool s_wifi_started;
static bool s_sta_connected;
static bool s_ap_active;
static esp_event_handler_instance_t s_wifi_any_id_handler;
static esp_event_handler_instance_t s_ip_got_ip_handler;

static void update_ip_string(esp_netif_t *netif)
{
    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char ip_buffer[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip_buffer, sizeof(ip_buffer));
        device_info_set_ip_address(ip_buffer);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        device_info_set_wifi_connected(false);
        device_info_set_ip_address(NULL);
        app_state_post_event(APP_EVENT_WIFI_DISCONNECTED);
        ESP_LOGW(TAG, "wifi disconnected");
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = (const wifi_event_ap_staconnected_t *)event_data;
        if (event != NULL) {
            ESP_LOGI(TAG, "station joined AP aid=%u mac=%02x:%02x:%02x:%02x:%02x:%02x",
                     event->aid,
                     event->mac[0], event->mac[1], event->mac[2],
                     event->mac[3], event->mac[4], event->mac[5]);
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = (const wifi_event_ap_stadisconnected_t *)event_data;
        if (event != NULL) {
            ESP_LOGW(TAG, "station left AP aid=%u mac=%02x:%02x:%02x:%02x:%02x:%02x reason=%u",
                     event->aid,
                     event->mac[0], event->mac[1], event->mac[2],
                     event->mac[3], event->mac[4], event->mac[5],
                     event->reason);
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "softAP started");
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_connected = true;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        device_info_set_wifi_connected(true);
        update_ip_string(s_sta_netif);
        app_state_post_event(APP_EVENT_WIFI_CONNECTED);
        ESP_LOGI(TAG, "wifi connected and got IP");
    }
}

static esp_err_t configure_softap_network(void)
{
    esp_netif_ip_info_t ip_info = {0};
    IP4_ADDR(&ip_info.ip, 192, 168, 6, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 6, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_RETURN_ON_ERROR(esp_netif_dhcps_stop(s_ap_netif), TAG, "stop dhcps failed");
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(s_ap_netif, &ip_info), TAG, "set AP ip failed");

    esp_netif_dns_info_t dns = {
        .ip.u_addr.ip4 = ip_info.ip,
        .ip.type = ESP_IPADDR_TYPE_V4,
    };
    ESP_RETURN_ON_ERROR(esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns), TAG, "set dns failed");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(s_ap_netif), TAG, "start dhcps failed");
    update_ip_string(s_ap_netif);
    return ESP_OK;
}

static void sta_connect_task(void *arg)
{
    (void)arg;
    wifi_credential_list_t list = {0};

    for (;;) {
        if (wifi_config_store_load(&list) != ESP_OK || list.count == 0) {
            ESP_LOGW(TAG, "no wifi configs available for STA loop");
            app_state_post_event(APP_EVENT_ENTER_PROVISIONING);
            break;
        }

        for (size_t index = 0; index < list.count; ++index) {
            wifi_config_t config = {0};
            strlcpy((char *)config.sta.ssid, list.items[index].ssid, sizeof(config.sta.ssid));
            strlcpy((char *)config.sta.password, list.items[index].password, sizeof(config.sta.password));
            config.sta.threshold.authmode = WIFI_AUTH_OPEN;
            config.sta.pmf_cfg.capable = true;
            config.sta.pmf_cfg.required = false;

            ESP_LOGI(TAG, "trying wifi[%u] ssid=%s", (unsigned)index, list.items[index].ssid);
            status_led_set(KENKO_LED_BLUE, LED_PATTERN_BREATHING);
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
            esp_wifi_disconnect();
            esp_wifi_connect();

            EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
                                                   pdMS_TO_TICKS(KENKO_WIFI_CONNECT_TIMEOUT_MS));
            if ((bits & WIFI_CONNECTED_BIT) != 0) {
                s_sta_task = NULL;
                vTaskDelete(NULL);
            }
        }

        ESP_LOGW(TAG, "all wifi configs failed, retrying from beginning");
    }

    s_sta_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t wifi_manager_init(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_events != NULL, ESP_ERR_NO_MEM, TAG, "event group alloc failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop create failed");
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_sta_netif != NULL && s_ap_netif != NULL, ESP_FAIL, TAG, "netif create failed");

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &s_wifi_any_id_handler), TAG, "wifi handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &s_ip_got_ip_handler), TAG, "ip handler failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi ram storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set initial STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");
    s_wifi_started = true;
    return ESP_OK;
}

esp_err_t wifi_manager_start_sta_loop(void)
{
    if (!s_wifi_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ap_active) {
        wifi_manager_stop_provisioning_ap();
    }

    if (s_sta_task == NULL) {
        xTaskCreate(sta_connect_task, "wifi_sta_loop", 6144, NULL, 5, &s_sta_task);
    }
    return ESP_OK;
}

esp_err_t wifi_manager_start_provisioning_ap(void)
{
    wifi_config_t config = {0};

    if (!s_wifi_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sta_task != NULL) {
        vTaskDelete(s_sta_task);
        s_sta_task = NULL;
    }

    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "stop wifi before AP mode failed");
    s_wifi_started = false;
    esp_wifi_disconnect();
    s_sta_connected = false;
    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
    device_info_set_wifi_connected(false);
    device_info_set_provisioning_mode(true);

    strlcpy((char *)config.ap.ssid, device_info_get_name(), sizeof(config.ap.ssid));
    config.ap.ssid_len = strlen((char *)config.ap.ssid);
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.pmf_cfg.required = false;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "set AP mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "set AP config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "restart wifi in AP mode failed");
    s_wifi_started = true;
    ESP_RETURN_ON_ERROR(configure_softap_network(), TAG, "configure AP network failed");
    s_ap_active = true;
    ESP_LOGI(TAG, "provisioning AP started ssid=%s ip=%s", config.ap.ssid, KENKO_AP_IP_ADDR);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_provisioning_ap(void)
{
    if (!s_ap_active) {
        return ESP_OK;
    }

    device_info_set_provisioning_mode(false);
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "stop wifi before STA mode failed");
    s_wifi_started = false;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "restore STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "restart wifi in STA mode failed");
    s_wifi_started = true;
    s_ap_active = false;
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_sta_connected;
}