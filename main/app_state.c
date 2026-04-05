#include "app_state.h"

#include <string.h>

#include "device_info.h"
#include "dns_captive.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mdns.h"
#include "status_led.h"
#include "web_server.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "app_state";
static QueueHandle_t s_event_queue;
static app_context_t *s_context;

static void stop_mdns(void)
{
    if (s_context->mdns_running) {
        mdns_free();
        s_context->mdns_running = false;
        ESP_LOGI(TAG, "mDNS stopped");
    }
}

static esp_err_t start_mdns(void)
{
    if (s_context->mdns_running) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mdns init failed");
    ESP_RETURN_ON_ERROR(mdns_hostname_set(device_info_get_mdns_hostname()), TAG, "mdns hostname failed");
    ESP_RETURN_ON_ERROR(mdns_instance_name_set(device_info_get_name()), TAG, "mdns instance failed");
    s_context->mdns_running = true;
    ESP_LOGI(TAG, "mDNS started hostname=%s.local", device_info_get_mdns_hostname());
    return ESP_OK;
}

static void enter_provisioning_mode(void)
{
    ESP_LOGI(TAG, "state -> provisioning");
    s_context->state = APP_STATE_PROVISIONING;
    s_context->provisioning_forced = true;
    stop_mdns();
    wifi_manager_start_provisioning_ap();
    dns_captive_start();
    web_server_start();
    status_led_set(KENKO_LED_BLUE, LED_PATTERN_SOLID);
}

static void start_wifi_connection_loop(void)
{
    ESP_LOGI(TAG, "state -> connecting_wifi");
    s_context->state = APP_STATE_CONNECTING_WIFI;
    s_context->provisioning_forced = false;
    dns_captive_stop();
    wifi_manager_stop_provisioning_ap();
    web_server_start();
    status_led_set(KENKO_LED_BLUE, LED_PATTERN_BREATHING);
    wifi_manager_start_sta_loop();
}

static void handle_wifi_connected(void)
{
    ESP_LOGI(TAG, "state -> running");
    s_context->state = APP_STATE_RUNNING;
    s_context->wifi_connected = true;
    web_server_start();
    start_mdns();
    status_led_set(KENKO_LED_GREEN, LED_PATTERN_SOLID);
}

static void handle_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "state -> wifi_disconnected");
    s_context->state = APP_STATE_WIFI_DISCONNECTED;
    s_context->wifi_connected = false;
    stop_mdns();
    start_wifi_connection_loop();
}

static void app_state_task(void *arg)
{
    (void)arg;
    app_event_t event;

    while (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
        switch (event) {
        case APP_EVENT_START:
            if (s_context->provisioning_forced || !wifi_config_store_has_entries()) {
                enter_provisioning_mode();
            } else {
                start_wifi_connection_loop();
            }
            break;
        case APP_EVENT_ENTER_PROVISIONING:
            enter_provisioning_mode();
            break;
        case APP_EVENT_WIFI_CONNECTED:
            handle_wifi_connected();
            break;
        case APP_EVENT_WIFI_DISCONNECTED:
            if (!s_context->provisioning_forced) {
                handle_wifi_disconnected();
            }
            break;
        case APP_EVENT_CLEAR_WIFI_CONFIG:
            wifi_config_store_clear();
            esp_restart();
            break;
        default:
            break;
        }
    }

    vTaskDelete(NULL);
}

esp_err_t app_state_start(app_context_t *context)
{
    if (context == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_context = context;
    s_event_queue = xQueueCreate(8, sizeof(app_event_t));
    ESP_RETURN_ON_FALSE(s_event_queue != NULL, ESP_ERR_NO_MEM, TAG, "event queue alloc failed");
    xTaskCreate(app_state_task, "app_state", 6144, NULL, 6, NULL);
    app_state_post_event(APP_EVENT_START);
    return ESP_OK;
}

void app_state_post_event(app_event_t event)
{
    if (s_event_queue == NULL) {
        return;
    }
    xQueueSend(s_event_queue, &event, 0);
}