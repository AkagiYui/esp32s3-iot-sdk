#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"

#ifndef TEST_WS2812_GPIO
#define TEST_WS2812_GPIO 48
#endif

#define TEST_WS2812_LED_COUNT 1
#define TEST_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static const char *TAG = "ws2812_smoke";

static led_strip_handle_t init_strip(void)
{
    led_strip_handle_t strip = NULL;

    led_strip_config_t strip_config = {
        .strip_gpio_num = TEST_WS2812_GPIO,
        .max_leds = TEST_WS2812_LED_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = TEST_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    ESP_ERROR_CHECK(led_strip_clear(strip));
    return strip;
}

static void show_color(led_strip_handle_t strip, uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(strip));
}

void app_main(void)
{
    led_strip_handle_t strip = init_strip();

    ESP_LOGI(TAG, "WS2812 smoke test started on GPIO %d", TEST_WS2812_GPIO);

    while (true) {
        show_color(strip, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        show_color(strip, 0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        show_color(strip, 0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(1000));

        show_color(strip, 255, 255, 255);
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_ERROR_CHECK(led_strip_clear(strip));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}