#include <string.h>

#include "app_state.h"
#include "button_monitor.h"
#include "device_info.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "status_led.h"
#include "storage_fs.h"
#include "wifi_manager.h"

static const char *TAG = "app_main";

void app_main(void)
{
    app_context_t context = {
        .provisioning_forced = false,
        .wifi_connected = false,
        .mdns_running = false,
        .state = APP_STATE_BOOT,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(device_info_init());
    ESP_ERROR_CHECK(status_led_init());
    status_led_set(KENKO_LED_ORANGE, LED_PATTERN_SOLID);
    ESP_LOGI(TAG, "device booted");

    ESP_ERROR_CHECK(storage_fs_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(button_monitor_start());
    ESP_ERROR_CHECK(app_state_start(&context));
}