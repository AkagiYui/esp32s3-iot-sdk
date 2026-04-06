#include "status_led.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "status_led";

static struct {
    led_strip_handle_t strip;
    led_hsv_t color;
    led_pattern_t pattern;
} s_led;

static led_rgb_t hsv_to_rgb(led_hsv_t hsv)
{
    led_rgb_t rgb = {0};

    if (hsv.saturation == 0) {
        rgb.red = hsv.value;
        rgb.green = hsv.value;
        rgb.blue = hsv.value;
        return rgb;
    }

    uint16_t hue = hsv.hue % 360;
    uint8_t region = hue / 60;
    uint16_t remainder = (hue % 60) * 255 / 60;

    uint16_t value = hsv.value;
    uint16_t saturation = hsv.saturation;
    uint8_t p = (uint8_t)(value * (255 - saturation) / 255);
    uint8_t q = (uint8_t)(value * (255 - (saturation * remainder) / 255) / 255);
    uint8_t t = (uint8_t)(value * (255 - (saturation * (255 - remainder)) / 255) / 255);

    switch (region) {
    case 0:
        rgb.red = (uint8_t)value;
        rgb.green = t;
        rgb.blue = p;
        break;
    case 1:
        rgb.red = q;
        rgb.green = (uint8_t)value;
        rgb.blue = p;
        break;
    case 2:
        rgb.red = p;
        rgb.green = (uint8_t)value;
        rgb.blue = t;
        break;
    case 3:
        rgb.red = p;
        rgb.green = q;
        rgb.blue = (uint8_t)value;
        break;
    case 4:
        rgb.red = t;
        rgb.green = p;
        rgb.blue = (uint8_t)value;
        break;
    default:
        rgb.red = (uint8_t)value;
        rgb.green = p;
        rgb.blue = q;
        break;
    }

    return rgb;
}

static uint8_t perceptual_breath_value(uint8_t phase)
{
    uint16_t mirrored = phase <= 127 ? phase : (uint16_t)(255 - phase);
    uint32_t normalized = mirrored * 2;

    /*
     * Use a smooth cubic ease-in/ease-out curve so the LED spends less time
     * changing rapidly near the darkest and brightest points.
     */
    uint32_t x = normalized;
    uint32_t x2 = (x * x) / 255;
    uint32_t x3 = (x2 * x) / 255;
    uint32_t eased = (3 * x2) - (2 * x3);
    if (eased > 255) {
        eased = 255;
    }

    /* Apply a light gamma-style shaping to better match perceived brightness. */
    uint32_t perceptual = (eased * eased) / 255;
    if (perceptual > 255) {
        perceptual = 255;
    }

    return (uint8_t)perceptual;
}

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
            led_rgb_t rgb = hsv_to_rgb(s_led.color);
            apply_color(rgb.red, rgb.green, rgb.blue);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for (int step = 0; step < 256; step += 4) {
            if (s_led.pattern != LED_PATTERN_BREATHING) {
                break;
            }
            uint8_t scale = perceptual_breath_value((uint8_t)step);
            led_hsv_t scaled_hsv = s_led.color;
            scaled_hsv.value = (uint8_t)(s_led.color.value * scale / 255);
            led_rgb_t rgb = hsv_to_rgb(scaled_hsv);
            apply_color(rgb.red, rgb.green, rgb.blue);
            vTaskDelay(pdMS_TO_TICKS(12));
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

void status_led_set(led_hsv_t color, led_pattern_t pattern)
{
    s_led.color = color;
    s_led.pattern = pattern;
}