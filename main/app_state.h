#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_PROVISIONING,
    APP_STATE_CONNECTING_WIFI,
    APP_STATE_RUNNING,
    APP_STATE_WIFI_DISCONNECTED,
} app_state_t;

typedef struct {
    bool provisioning_forced;
    bool wifi_connected;
    bool mdns_running;
    app_state_t state;
} app_context_t;

typedef enum {
    APP_EVENT_START = 0,
    APP_EVENT_ENTER_PROVISIONING,
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_DISCONNECTED,
    APP_EVENT_CLEAR_WIFI_CONFIG,
} app_event_t;

esp_err_t app_state_start(app_context_t *context);
void app_state_post_event(app_event_t event);