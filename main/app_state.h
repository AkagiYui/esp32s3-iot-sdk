#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_PROVISIONING, /**< SoftAP + captive portal，等待用户填写 WiFi */
    APP_STATE_CONNECTING,   /**< 正在按优先级逐个尝试已保存的配置 */
    APP_STATE_ONLINE,       /**< 已连上路由器并拿到 IP */
    APP_STATE_OFFLINE,      /**< 曾经在线但已掉线，正在重连 */
} app_state_t;

typedef enum {
    APP_EVENT_START = 0,
    APP_EVENT_ENTER_PROVISIONING,
    APP_EVENT_APPLY_WIFI_CONFIG, /**< 配置被改写，立刻用新配置重连 */
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_LOST,
    APP_EVENT_SETTINGS_CHANGED,
    APP_EVENT_REBOOT,
    APP_EVENT_FACTORY_RESET,
} app_event_t;

/** 启动状态机任务。上下文是模块内的静态实例，不依赖调用方的栈。 */
esp_err_t app_state_start(void);

/** 投递事件；队列满时丢弃并记日志，绝不阻塞调用方（可能是 ISR 之外的任意任务）。 */
void app_state_post_event(app_event_t event);

app_state_t app_state_get(void);

const char *app_state_name(app_state_t state);

/** 设备当前是否处于配网模式。 */
bool app_state_is_provisioning(void);
