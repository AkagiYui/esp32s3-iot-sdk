#pragma once

#include <stdint.h>

/** LED 显示模式。 */
typedef enum {
    LED_PATTERN_SOLID = 0, /**< 常亮 */
    LED_PATTERN_BREATHING, /**< 呼吸 */
    LED_PATTERN_BLINK,     /**< 闪烁 */
} led_pattern_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_rgb_t;

typedef struct {
    uint16_t hue;       /**< 0..359 */
    uint8_t saturation; /**< 0..255 */
    uint8_t value;      /**< 0..255 */
} led_hsv_t;

/** HSV -> RGB，纯整数运算，不依赖 ESP-IDF，可在 host 上单测。 */
led_rgb_t led_color_hsv_to_rgb(led_hsv_t hsv);

/**
 * 呼吸曲线：把 0..255 的相位映射为 0..255 的亮度系数。
 * 相位在 128 处折返，并叠加缓入缓出与伽马整形，使亮度变化更贴近人眼感受。
 */
uint8_t led_color_breath_scale(uint8_t phase);

/** 按 0..255 的系数缩放 HSV 的明度。 */
led_hsv_t led_color_scale_value(led_hsv_t hsv, uint8_t scale);
