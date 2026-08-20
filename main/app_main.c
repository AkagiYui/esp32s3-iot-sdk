#include "app_state.h"
#include "button_monitor.h"
#include "device_info.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "kenko_core.h"
#include "nvs_flash.h"
#include "ota_service.h"
#include "settings_store.h"
#include "status_led.h"
#include "storage_fs.h"
#include "time_sync.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "app_main";

/**
 * NVS 页写满或版本变更时 `nvs_flash_init()` 会返回错误。
 * 直接 ESP_ERROR_CHECK 会让设备卡在开机重启循环里，正确做法是擦掉重来。
 */
static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs needs erase (%s), reformatting", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    /* 事件循环要先于任何会投递事件的组件建立。 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(device_info_init());
    ESP_ERROR_CHECK(status_led_init());
    status_led_set(KENKO_LED_ORANGE, LED_PATTERN_SOLID);

    /* 文件系统缺失不是致命错误：设备仍要能启动并把问题显示出来。 */
    ESP_ERROR_CHECK(storage_fs_init());

    const device_identity_t *identity = device_info_identity();
    ESP_ERROR_CHECK(settings_store_init(identity->default_name));

    app_settings_t settings;
    settings_store_get(&settings);
    status_led_set_brightness(settings.led_brightness);
    ESP_ERROR_CHECK(time_sync_init());

    ESP_ERROR_CHECK(wifi_config_store_init());
    ESP_ERROR_CHECK(ota_service_init());
    ESP_ERROR_CHECK(wifi_manager_init(identity->mdns_hostname));
    ESP_ERROR_CHECK(button_monitor_start());
    ESP_ERROR_CHECK(app_state_start());

    ESP_LOGI(TAG, "boot complete, device=%s", settings.device_name);
}
