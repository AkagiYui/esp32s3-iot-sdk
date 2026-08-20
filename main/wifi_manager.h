#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
} wifi_scan_entry_t;

typedef struct {
    bool sta_connected;
    bool ap_active;
    bool connecting;
    const char *mode;
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char ap_ip[16];
    uint8_t ap_clients;
} wifi_status_t;

esp_err_t wifi_manager_init(void);

/**
 * 启动 STA 连接循环：按保存顺序逐个尝试，一轮全失败后退避再来。
 * 没有任何已保存配置时会投递 APP_EVENT_ENTER_PROVISIONING。
 */
esp_err_t wifi_manager_start_sta_loop(void);

/** 请求停止连接循环，并等待任务真正退出。 */
void wifi_manager_stop_sta_loop(void);

/**
 * 进入配网模式。
 *
 * 使用 APSTA 而不是纯 AP：`esp_wifi_scan_start()` 需要 STA 接口处于启用状态，
 * 而扫描周边热点恰恰是配网页面最需要的能力。
 */
esp_err_t wifi_manager_start_provisioning_ap(void);

/** 退出配网模式，回到纯 STA。 */
esp_err_t wifi_manager_stop_provisioning_ap(void);

bool wifi_manager_is_connected(void);

void wifi_manager_get_status(wifi_status_t *out);

/**
 * 扫描周边热点。结果带 TTL 缓存，重复请求不会反复打断连接。
 *
 * @param out    输出数组
 * @param max    输出数组容量
 * @param count  实际写入条数
 * @param force  true 时忽略缓存强制重扫
 */
esp_err_t wifi_manager_scan(wifi_scan_entry_t *out, size_t max, size_t *count, bool force);

/** 认证模式的可读名称。 */
const char *wifi_manager_authmode_name(wifi_auth_mode_t authmode);
