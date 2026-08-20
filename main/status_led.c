#include "status_led.h"

#include "app_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "status_led";

#define LED_TICK_MS 20
#define LED_BREATH_STEP 6
#define LED_BLINK_HALF_PERIOD_TICKS (200 / LED_TICK_MS)

static struct {
    led_strip_handle_t strip;
    SemaphoreHandle_t lock;
    led_hsv_t color;
    led_pattern_t pattern;
    uint8_t brightness; /* 0..100 */
} s_led;

/** 取一份状态快照，避免 LED 任务读到被其它任务改到一半的结构体。 */
static void snapshot(led_hsv_t *color, led_pattern_t *pattern, uint8_t *brightness)
{
    xSemaphoreTake(s_led.lock, portMAX_DELAY);
    *color = s_led.color;
    *pattern = s_led.pattern;
    *brightness = s_led.brightness;
    xSemaphoreGive(s_led.lock);
}

static void apply_color(led_rgb_t rgb)
{
    static led_rgb_t last;
    static bool has_last;

    if (has_last && last.red == rgb.red && last.green == rgb.green && last.blue == rgb.blue) {
        return;
    }

    if (led_strip_set_pixel(s_led.strip, 0, rgb.red, rgb.green, rgb.blue) == ESP_OK &&
        led_strip_refresh(s_led.strip) == ESP_OK) {
        last = rgb;
        has_last = true;
    }
}

static void led_task(void *arg)
{
    (void)arg;
    uint32_t tick = 0;

    for (;;) {
        led_hsv_t color;
        led_pattern_t pattern;
        uint8_t brightness;
        snapshot(&color, &pattern, &brightness);

        uint8_t scale = 255;
        switch (pattern) {
        case LED_PATTERN_BREATHING:
            scale = led_color_breath_scale((uint8_t)((tick * LED_BREATH_STEP) & 0xff));
            break;
        case LED_PATTERN_BLINK:
            scale = ((tick / LED_BLINK_HALF_PERIOD_TICKS) % 2 == 0) ? 255 : 0;
            break;
        case LED_PATTERN_SOLID:
        default:
            scale = 255;
            break;
        }

        /* 用户亮度设置与模式亮度相乘，两者都作用在 HSV 的 value 上。 */
        uint16_t combined = (uint16_t)scale * brightness / 100;
        led_hsv_t shaped = led_color_scale_value(color, (uint8_t)combined);
        apply_color(led_color_hsv_to_rgb(shaped));

        tick++;
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));
    }
}

esp_err_t status_led_init(void)
{
    s_led.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_led.lock != NULL, ESP_ERR_NO_MEM, TAG, "led mutex alloc failed");

    const led_strip_config_t strip_config = {
        .strip_gpio_num = KENKO_WS2812_GPIO,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led.strip), TAG,
                        "create strip failed");

    s_led.color = KENKO_LED_ORANGE;
    s_led.pattern = LED_PATTERN_SOLID;
    s_led.brightness = 100;

    BaseType_t created =
        xTaskCreate(led_task, "status_led", KENKO_TASK_STACK_LED, NULL, KENKO_TASK_PRIORITY_LED, NULL);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "led task create failed");

    ESP_LOGI(TAG, "status LED initialized on GPIO %d", KENKO_WS2812_GPIO);
    return ESP_OK;
}

void status_led_set(led_hsv_t color, led_pattern_t pattern)
{
    if (s_led.lock == NULL) {
        return;
    }

    xSemaphoreTake(s_led.lock, portMAX_DELAY);
    s_led.color = color;
    s_led.pattern = pattern;
    xSemaphoreGive(s_led.lock);
}

void status_led_set_brightness(uint8_t percent)
{
    if (s_led.lock == NULL) {
        return;
    }

    xSemaphoreTake(s_led.lock, portMAX_DELAY);
    s_led.brightness = percent > 100 ? 100 : percent;
    xSemaphoreGive(s_led.lock);
}
