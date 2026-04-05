#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_sta_loop(void);
esp_err_t wifi_manager_start_provisioning_ap(void);
esp_err_t wifi_manager_stop_provisioning_ap(void);
bool wifi_manager_is_connected(void);