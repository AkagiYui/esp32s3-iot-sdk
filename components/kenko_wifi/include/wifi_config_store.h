#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "sdkconfig.h"

/* 含结尾 NUL：SSID 最长 32 字节，WPA2 密码最长 63 字节。 */
#define WIFI_CONFIG_SSID_MAX_LEN 33
#define WIFI_CONFIG_PASSWORD_MAX_LEN 65
#define WIFI_CONFIG_MAX_ITEMS CONFIG_KENKO_WIFI_MAX_SAVED_NETWORKS

typedef struct {
    char ssid[WIFI_CONFIG_SSID_MAX_LEN];
    char password[WIFI_CONFIG_PASSWORD_MAX_LEN];
} wifi_credential_t;

/** 列表顺序即连接优先级。 */
typedef struct {
    size_t count;
    wifi_credential_t items[WIFI_CONFIG_MAX_ITEMS];
} wifi_credential_list_t;

/** 从 LittleFS 载入配置到内存缓存；文件缺失视为空列表。 */
esp_err_t wifi_config_store_init(void);

/** 取内存缓存的快照。 */
void wifi_config_store_load(wifi_credential_list_t *list);

/** 覆盖写入并落盘。 */
esp_err_t wifi_config_store_save(const wifi_credential_list_t *list);

bool wifi_config_store_has_entries(void);

/** 清空配置并落盘。 */
esp_err_t wifi_config_store_clear(void);

/**
 * 按 SSID 取回已保存的密码，供“不修改密码”的更新流程使用。
 * 找到返回 true。
 */
bool wifi_config_store_find_password(const char *ssid, char *out, size_t out_size);
