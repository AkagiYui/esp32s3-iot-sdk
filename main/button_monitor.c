#include "button_monitor.h"

#include "app_config.h"
#include "app_state.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "button";

/* 连续读到相同电平的次数达到阈值才认为电平稳定。 */
#define BUTTON_DEBOUNCE_SAMPLES 3

static void button_task(void *arg)
{
    (void)arg;

    bool stable_pressed = false;
    bool last_sample = false;
    int same_sample_count = 0;
    uint32_t pressed_ms = 0;
    bool provisioning_sent = false;
    bool factory_reset_sent = false;

    for (;;) {
        bool sample = gpio_get_level(KENKO_BUTTON_GPIO) == 0;

        if (sample == last_sample) {
            if (same_sample_count < BUTTON_DEBOUNCE_SAMPLES) {
                same_sample_count++;
            }
        } else {
            same_sample_count = 1;
            last_sample = sample;
        }

        if (same_sample_count >= BUTTON_DEBOUNCE_SAMPLES && stable_pressed != sample) {
            stable_pressed = sample;
            pressed_ms = 0;
            provisioning_sent = false;
            factory_reset_sent = false;
            ESP_LOGD(TAG, "button %s", stable_pressed ? "pressed" : "released");
        }

        if (stable_pressed) {
            pressed_ms += KENKO_BUTTON_POLL_INTERVAL_MS;

            if (!provisioning_sent && pressed_ms >= KENKO_BUTTON_PROVISIONING_MS) {
                ESP_LOGW(TAG, "long press %u ms: entering provisioning mode",
                         (unsigned)KENKO_BUTTON_PROVISIONING_MS);
                app_state_post_event(APP_EVENT_ENTER_PROVISIONING);
                provisioning_sent = true;
            }

            if (!factory_reset_sent && pressed_ms >= KENKO_BUTTON_FACTORY_RESET_MS) {
                ESP_LOGW(TAG, "long press %u ms: factory reset", (unsigned)KENKO_BUTTON_FACTORY_RESET_MS);
                app_state_post_event(APP_EVENT_FACTORY_RESET);
                factory_reset_sent = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(KENKO_BUTTON_POLL_INTERVAL_MS));
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

    BaseType_t created = xTaskCreate(button_task, "button_monitor", KENKO_TASK_STACK_BUTTON, NULL,
                                     KENKO_TASK_PRIORITY_BUTTON, NULL);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "button task create failed");
    return ESP_OK;
}
