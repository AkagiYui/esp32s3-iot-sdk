#include "button_monitor.h"

#include "app_config.h"
#include "app_state.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_config_store.h"

static const char *TAG = "button";

static void button_task(void *arg)
{
    (void)arg;
    TickType_t pressed_at = 0;
    bool sent_provisioning = false;

    for (;;) {
        bool pressed = gpio_get_level(KENKO_BUTTON_GPIO) == 0;
        if (pressed) {
            if (pressed_at == 0) {
                pressed_at = xTaskGetTickCount();
                sent_provisioning = false;
            }

            uint32_t elapsed_ms = (uint32_t)((xTaskGetTickCount() - pressed_at) * portTICK_PERIOD_MS);
            if (!sent_provisioning && elapsed_ms >= 5000 && elapsed_ms < 10000) {
                ESP_LOGW(TAG, "boot button long press: enter provisioning mode");
                app_state_post_event(APP_EVENT_ENTER_PROVISIONING);
                sent_provisioning = true;
            }

            if (elapsed_ms >= 10000) {
                ESP_LOGW(TAG, "boot button very long press: clear wifi config and reboot");
                wifi_config_store_clear();
                esp_restart();
            }
        } else {
            pressed_at = 0;
            sent_provisioning = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t button_monitor_start(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << KENKO_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "button gpio config failed");
    xTaskCreate(button_task, "button_monitor", 4096, NULL, 5, NULL);
    return ESP_OK;
}