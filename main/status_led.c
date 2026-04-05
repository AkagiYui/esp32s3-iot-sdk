#include "status_led.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "status_led";

static struct {
    led_strip_handle_t strip;
    led_rgb_t color;
    led_pattern_t pattern;
} s_led;

static void apply_color(uint8_t red, uint8_t green, uint8_t blue)
{
    led_strip_set_pixel(s_led.strip, 0, red, green, blue);
    led_strip_refresh(s_led.strip);
}

static void led_task(void *arg)
{
    (void)arg;

    for (;;) {
        if (s_led.pattern == LED_PATTERN_SOLID) {
            apply_color(s_led.color.red, s_led.color.green, s_led.color.blue);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for (int step = 0; step <= 100; ++step) {
            if (s_led.pattern != LED_PATTERN_BREATHING) {
                break;
            }
            uint8_t scale = (uint8_t)((step <= 50 ? step : 100 - step) * 255 / 50);
            apply_color((uint8_t)(s_led.color.red * scale / 255),
                        (uint8_t)(s_led.color.green * scale / 255),
                        (uint8_t)(s_led.color.blue * scale / 255));
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

esp_err_t status_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = KENKO_WS2812_GPIO,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led.strip), TAG, "create strip failed");
    s_led.color = KENKO_LED_ORANGE;
    s_led.pattern = LED_PATTERN_SOLID;
    xTaskCreate(led_task, "status_led", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "status LED initialized");
    return ESP_OK;
}

void status_led_set(led_rgb_t color, led_pattern_t pattern)
{
    s_led.color = color;
    s_led.pattern = pattern;
}