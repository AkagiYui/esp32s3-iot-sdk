#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_event.h"

/**
 * 组件之间的公共依赖：事件基与共享运行状态。
 *
 * 各功能组件（wifi / web / board）只往这里投事件、从这里读状态，
 * 不反向依赖应用层的状态机，避免出现组件与 main 互相引用的环。
 */

ESP_EVENT_DECLARE_BASE(KENKO_EVENT);

typedef enum {
    KENKO_EVENT_START = 0,
    KENKO_EVENT_ENTER_PROVISIONING, /**< 请求进入配网模式 */
    KENKO_EVENT_APPLY_WIFI_CONFIG,  /**< 用已保存的配置重新连接 */
    KENKO_EVENT_WIFI_CONNECTED,     /**< STA 已拿到 IP */
    KENKO_EVENT_WIFI_LOST,          /**< 已连接的链路断开 */
    KENKO_EVENT_SETTINGS_CHANGED,   /**< 设备设置被改写 */
    KENKO_EVENT_REBOOT,             /**< 请求重启 */
    KENKO_EVENT_FACTORY_RESET,      /**< 请求恢复出厂设置 */
} kenko_event_id_t;

typedef enum {
    KENKO_STATE_BOOT = 0,
    KENKO_STATE_PROVISIONING, /**< SoftAP + captive portal，等待用户填写 WiFi */
    KENKO_STATE_CONNECTING,   /**< 正在按优先级逐个尝试已保存的配置 */
    KENKO_STATE_ONLINE,       /**< 已连上路由器并拿到 IP */
    KENKO_STATE_OFFLINE,      /**< 曾经在线但已掉线，正在重连 */
} kenko_state_t;

/** 投递一个无附加数据的事件；失败只记日志，绝不阻塞调用方。 */
esp_err_t kenko_event_post(kenko_event_id_t event_id);

/** 由状态机在状态迁移时调用。 */
void kenko_state_set(kenko_state_t state);

kenko_state_t kenko_state_get(void);

const char *kenko_state_name(kenko_state_t state);

bool kenko_state_is_provisioning(void);
