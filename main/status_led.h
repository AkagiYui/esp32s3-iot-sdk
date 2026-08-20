#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "led_color.h"

esp_err_t status_led_init(void);

/** 设置颜色与显示模式，可从任意任务调用。 */
void status_led_set(led_hsv_t color, led_pattern_t pattern);

/** 设置整体亮度百分比（0..100），0 表示熄灭状态灯。 */
void status_led_set_brightness(uint8_t percent);
