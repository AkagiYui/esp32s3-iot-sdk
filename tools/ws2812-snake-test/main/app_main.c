#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"

/* ==================== 可调参数 ==================== */
#define SNAKE_GPIO         18
#define SNAKE_LED_COUNT    256
#define SNAKE_LENGTH       40      // 蛇身长度（亮灯个数）
#define SNAKE_SPEED_MS     40      // 每帧间隔毫秒
#define HUE_STEP           3       // 每帧色相变化步长
#define RMT_RESOLUTION_HZ  (10 * 1000 * 1000)

static const char *TAG = "snake";

/* ==================== HSV → RGB ==================== */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    h %= 360;
    uint8_t region = h / 60;
    uint8_t rem = (uint8_t)((h % 60) * 255 / 60);

    uint8_t p = (uint8_t)(v * (255 - s) / 255);
    uint8_t q = (uint8_t)(v * (255 - s * rem / 255) / 255);
    uint8_t t = (uint8_t)(v * (255 - s * (255 - rem) / 255) / 255);

    switch (region) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default:*r = v; *g = p; *b = q; break;
    }
}

/* ==================== 主程序 ==================== */
void app_main(void)
{
    ESP_LOGI(TAG, "=== WS2812 Snake Test ===");
    ESP_LOGI(TAG, "GPIO: %d | LEDs: %d | Snake length: %d",
             SNAKE_GPIO, SNAKE_LED_COUNT, SNAKE_LENGTH);

    /* 初始化灯带 */
    led_strip_handle_t strip = NULL;

    led_strip_config_t strip_config = {
        .strip_gpio_num = SNAKE_GPIO,
        .max_leds = SNAKE_LED_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .flags.with_dma = true,   // DMA 模式，支持大量 LED
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    ESP_ERROR_CHECK(led_strip_clear(strip));
    ESP_LOGI(TAG, "Strip initialized, starting snake animation...");

    int head_pos = 0;
    uint16_t hue = 0;

    while (true) {
        /* 先清除全部 */
        for (int i = 0; i < SNAKE_LED_COUNT; i++) {
            led_strip_set_pixel(strip, i, 0, 0, 0);
        }

        /* 从尾到头画蛇身：靠近尾部越来越暗 */
        for (int i = 0; i < SNAKE_LENGTH; i++) {
            int pos = (head_pos - i + SNAKE_LED_COUNT) % SNAKE_LED_COUNT;

            /* 亮度：头部最亮，尾部平方衰减 */
            uint16_t raw = (uint16_t)(SNAKE_LENGTH - i) * 255 / SNAKE_LENGTH;
            uint8_t brightness = (uint8_t)(raw * raw / 255);

            uint8_t r, g, b;
            hsv_to_rgb(hue, 255, brightness, &r, &g, &b);
            led_strip_set_pixel(strip, pos, r, g, b);
        }

        led_strip_refresh(strip);

        head_pos = (head_pos + 1) % SNAKE_LED_COUNT;
        hue = (hue + HUE_STEP) % 360;

        vTaskDelay(pdMS_TO_TICKS(SNAKE_SPEED_MS));
    }
}
