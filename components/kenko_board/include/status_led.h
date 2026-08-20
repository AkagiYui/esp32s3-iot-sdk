#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "led_color.h"

/* 状态指示灯的配色。设备状态与颜色的对应关系由应用层决定。 */
#define KENKO_LED_ORANGE ((led_hsv_t){.hue = 24, .saturation = 255, .value = 255})
#define KENKO_LED_BLUE ((led_hsv_t){.hue = 225, .saturation = 255, .value = 255})
#define KENKO_LED_GREEN ((led_hsv_t){.hue = 128, .saturation = 255, .value = 255})
#define KENKO_LED_PURPLE ((led_hsv_t){.hue = 285, .saturation = 255, .value = 255})
#define KENKO_LED_RED ((led_hsv_t){.hue = 0, .saturation = 255, .value = 255})

esp_err_t status_led_init(void);

/** 设置颜色与显示模式，可从任意任务调用。 */
void status_led_set(led_hsv_t color, led_pattern_t pattern);

/** 设置整体亮度百分比（0..100），0 表示熄灭状态灯。 */
void status_led_set_brightness(uint8_t percent);
