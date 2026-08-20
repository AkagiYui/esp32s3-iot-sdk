#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define SETTINGS_DEVICE_NAME_MAX_LEN 33
#define SETTINGS_TIMEZONE_MAX_LEN 40

/** 用户可修改的设备设置，持久化在 LittleFS 的 settings.json。 */
typedef struct {
    char device_name[SETTINGS_DEVICE_NAME_MAX_LEN];
    char timezone[SETTINGS_TIMEZONE_MAX_LEN]; /**< POSIX TZ 串，例如 CST-8 */
    bool ntp_enabled;
    uint8_t led_brightness; /**< 0..100，0 表示关闭状态灯 */
} app_settings_t;

/**
 * 载入设置文件；文件缺失或损坏时回落到默认值并落盘。
 * @param default_device_name 出厂默认设备名。
 */
esp_err_t settings_store_init(const char *default_device_name);

/** 取当前设置的快照。 */
void settings_store_get(app_settings_t *out);

/** 校验并保存设置；任一字段非法时整体拒绝，返回 ESP_ERR_INVALID_ARG。 */
esp_err_t settings_store_update(const app_settings_t *settings);

/** 恢复默认设置并落盘。 */
esp_err_t settings_store_reset(void);

/** 校验设置是否合法，供接口层在写入前给出精确的错误信息。 */
bool settings_store_validate(const app_settings_t *settings, const char **reason);
