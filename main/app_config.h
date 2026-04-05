#pragma once

#include <stdint.h>

#include "firmware_version.h"

#define KENKO_DEVICE_PREFIX "kenko32"
#define KENKO_AP_IP_ADDR "192.168.6.1"
#define KENKO_AP_GATEWAY_ADDR "192.168.6.1"
#define KENKO_AP_NETMASK_ADDR "255.255.255.0"
#define KENKO_STORAGE_BASE_PATH "/storage"
#define KENKO_WEB_BASE_PATH "/web"
#define KENKO_WIFI_CONFIG_FILE KENKO_STORAGE_BASE_PATH "/wifi_config.json"
#define KENKO_WIFI_CONNECT_TIMEOUT_MS 15000
#define KENKO_BUTTON_GPIO 0
#define KENKO_WS2812_GPIO 47
#define KENKO_HTTP_PORT 80
#define KENKO_DNS_PORT 53
#define KENKO_FIRMWARE_VERSION_INT KENKO_FIRMWARE_VERSION
#define KENKO_FIRMWARE_VERSION_NAME KENKO_FIRMWARE_NAME

typedef enum {
    LED_PATTERN_SOLID = 0,
    LED_PATTERN_BREATHING,
} led_pattern_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_rgb_t;

typedef struct {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
} led_hsv_t;

static const led_hsv_t KENKO_LED_ORANGE = { .hue = 24, .saturation = 255, .value = 255 };
static const led_hsv_t KENKO_LED_BLUE = { .hue = 225, .saturation = 255, .value = 255 };
static const led_hsv_t KENKO_LED_GREEN = { .hue = 128, .saturation = 255, .value = 255 };