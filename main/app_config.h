#pragma once

#include <stdint.h>

#include "firmware_version.h"
#include "led_color.h"

/* ---- 设备标识 ---- */
#define KENKO_DEVICE_PREFIX "kenko32"
#define KENKO_FIRMWARE_VERSION_INT KENKO_FIRMWARE_VERSION
#define KENKO_FIRMWARE_VERSION_NAME KENKO_FIRMWARE_NAME

/* ---- SoftAP 网络（唯一真源，字符串形式由八位组拼出）---- */
#define KENKO_AP_IP0 192
#define KENKO_AP_IP1 168
#define KENKO_AP_IP2 6
#define KENKO_AP_IP3 1
#define KENKO_AP_NETMASK0 255
#define KENKO_AP_NETMASK1 255
#define KENKO_AP_NETMASK2 255
#define KENKO_AP_NETMASK3 0

#define KENKO__STRINGIFY(value) #value
#define KENKO__EXPAND(value) KENKO__STRINGIFY(value)
#define KENKO_AP_IP_ADDR        \
    KENKO__EXPAND(KENKO_AP_IP0) \
    "." KENKO__EXPAND(KENKO_AP_IP1) "." KENKO__EXPAND(KENKO_AP_IP2) "." KENKO__EXPAND(KENKO_AP_IP3)
#define KENKO_AP_URL "http://" KENKO_AP_IP_ADDR "/"
#define KENKO_AP_CHANNEL 1
#define KENKO_AP_MAX_CONNECTION 4

/* ---- 文件系统 ---- */
#define KENKO_STORAGE_BASE_PATH "/storage"
#define KENKO_WEB_BASE_PATH "/web"
#define KENKO_STORAGE_PARTITION "storage"
#define KENKO_WEB_PARTITION "web"
#define KENKO_WIFI_CONFIG_FILE KENKO_STORAGE_BASE_PATH "/wifi_config.json"
#define KENKO_SETTINGS_FILE KENKO_STORAGE_BASE_PATH "/settings.json"

/* ---- 外设 ---- */
#define KENKO_BUTTON_GPIO 0
#define KENKO_WS2812_GPIO 48

/* 长按阈值：短于第一档忽略，第一档进配网，第二档清空配置并重启。 */
#define KENKO_BUTTON_POLL_INTERVAL_MS 50
#define KENKO_BUTTON_PROVISIONING_MS 5000
#define KENKO_BUTTON_FACTORY_RESET_MS 10000

/* ---- 网络行为 ---- */
#define KENKO_HTTP_PORT 80
#define KENKO_DNS_PORT 53
#define KENKO_DNS_TTL_SECONDS 60
#define KENKO_WIFI_CONNECT_TIMEOUT_MS 15000
/* 一轮把所有已存配置都试过之后的退避，避免无限全速重试。 */
#define KENKO_WIFI_RETRY_BACKOFF_MS 10000
#define KENKO_WIFI_SCAN_CACHE_TTL_MS 15000
#define KENKO_WIFI_SCAN_MAX_RESULTS 32

/* ---- 时间同步 ---- */
#define KENKO_DEFAULT_TIMEZONE "CST-8"
#define KENKO_SNTP_SERVER_PRIMARY "ntp.aliyun.com"
#define KENKO_SNTP_SERVER_SECONDARY "pool.ntp.org"

/* ---- 任务参数（集中定义，便于评估栈用量与优先级关系）---- */
#define KENKO_TASK_PRIORITY_STATE 6
#define KENKO_TASK_PRIORITY_WIFI 5
#define KENKO_TASK_PRIORITY_BUTTON 5
#define KENKO_TASK_PRIORITY_DNS 4
#define KENKO_TASK_PRIORITY_LED 4

#define KENKO_TASK_STACK_STATE 4096
#define KENKO_TASK_STACK_WIFI 4096
#define KENKO_TASK_STACK_BUTTON 3072
#define KENKO_TASK_STACK_DNS 4096
#define KENKO_TASK_STACK_LED 3072
#define KENKO_TASK_STACK_HTTPD 8192

/* ---- 状态指示灯配色 ---- */
#define KENKO_LED_ORANGE ((led_hsv_t){.hue = 24, .saturation = 255, .value = 255})
#define KENKO_LED_BLUE ((led_hsv_t){.hue = 225, .saturation = 255, .value = 255})
#define KENKO_LED_GREEN ((led_hsv_t){.hue = 128, .saturation = 255, .value = 255})
#define KENKO_LED_PURPLE ((led_hsv_t){.hue = 285, .saturation = 255, .value = 255})
#define KENKO_LED_RED ((led_hsv_t){.hue = 0, .saturation = 255, .value = 255})
