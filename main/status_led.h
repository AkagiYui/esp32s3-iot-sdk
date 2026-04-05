#pragma once

#include "esp_err.h"

#include "app_config.h"

esp_err_t status_led_init(void);
void status_led_set(led_rgb_t color, led_pattern_t pattern);