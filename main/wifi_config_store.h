#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#define WIFI_CONFIG_SSID_MAX_LEN 33
#define WIFI_CONFIG_PASSWORD_MAX_LEN 65
#define WIFI_CONFIG_MAX_ITEMS 16

typedef struct {
    char ssid[WIFI_CONFIG_SSID_MAX_LEN];
    char password[WIFI_CONFIG_PASSWORD_MAX_LEN];
} wifi_credential_t;

typedef struct {
    size_t count;
    wifi_credential_t items[WIFI_CONFIG_MAX_ITEMS];
} wifi_credential_list_t;

esp_err_t wifi_config_store_load(wifi_credential_list_t *list);
esp_err_t wifi_config_store_save(const wifi_credential_list_t *list);
esp_err_t wifi_config_store_add(const char *ssid, const char *password);
esp_err_t wifi_config_store_remove(size_t index);
bool wifi_config_store_has_entries(void);
esp_err_t wifi_config_store_clear(void);