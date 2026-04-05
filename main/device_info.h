#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char device_name[32];
    char mdns_hostname[40];
    char mac_suffix[5];
    uint32_t firmware_version;
    char firmware_name[16];
    uint32_t free_heap;
    uint32_t min_free_heap;
    bool wifi_connected;
    bool provisioning_mode;
    char ip_address[16];
} device_info_t;

esp_err_t device_info_init(void);
const char *device_info_get_name(void);
const char *device_info_get_mdns_hostname(void);
const char *device_info_get_mac_suffix(void);
void device_info_set_wifi_connected(bool connected);
void device_info_set_provisioning_mode(bool enabled);
void device_info_set_ip_address(const char *ip_address);
void device_info_snapshot(device_info_t *info);