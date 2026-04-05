#include "device_info.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "device_info";

static struct {
    bool initialized;
    bool provisioning_mode;
    bool wifi_connected;
    char device_name[32];
    char mdns_hostname[40];
    char mac_suffix[5];
    char ip_address[16];
} s_device_state;

esp_err_t device_info_init(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        return err;
    }

    snprintf(s_device_state.mac_suffix, sizeof(s_device_state.mac_suffix), "%02x%02x", mac[4], mac[5]);
    snprintf(s_device_state.device_name, sizeof(s_device_state.device_name), "%s-%s", KENKO_DEVICE_PREFIX, s_device_state.mac_suffix);
    snprintf(s_device_state.mdns_hostname, sizeof(s_device_state.mdns_hostname), "%s-%s", KENKO_DEVICE_PREFIX, s_device_state.mac_suffix);
    strlcpy(s_device_state.ip_address, "0.0.0.0", sizeof(s_device_state.ip_address));
    s_device_state.initialized = true;

    ESP_LOGI(TAG, "device_name=%s firmware_version=%u firmware_name=%s",
             s_device_state.device_name,
             (unsigned)KENKO_FIRMWARE_VERSION_INT,
             KENKO_FIRMWARE_VERSION_NAME);
    return ESP_OK;
}

const char *device_info_get_name(void)
{
    return s_device_state.device_name;
}

const char *device_info_get_mdns_hostname(void)
{
    return s_device_state.mdns_hostname;
}

const char *device_info_get_mac_suffix(void)
{
    return s_device_state.mac_suffix;
}

void device_info_set_wifi_connected(bool connected)
{
    s_device_state.wifi_connected = connected;
}

void device_info_set_provisioning_mode(bool enabled)
{
    s_device_state.provisioning_mode = enabled;
}

void device_info_set_ip_address(const char *ip_address)
{
    if (ip_address == NULL) {
        strlcpy(s_device_state.ip_address, "0.0.0.0", sizeof(s_device_state.ip_address));
        return;
    }

    strlcpy(s_device_state.ip_address, ip_address, sizeof(s_device_state.ip_address));
}

void device_info_snapshot(device_info_t *info)
{
    if (info == NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));
    strlcpy(info->device_name, s_device_state.device_name, sizeof(info->device_name));
    strlcpy(info->mdns_hostname, s_device_state.mdns_hostname, sizeof(info->mdns_hostname));
    strlcpy(info->mac_suffix, s_device_state.mac_suffix, sizeof(info->mac_suffix));
    strlcpy(info->firmware_name, KENKO_FIRMWARE_VERSION_NAME, sizeof(info->firmware_name));
    strlcpy(info->ip_address, s_device_state.ip_address, sizeof(info->ip_address));
    info->firmware_version = KENKO_FIRMWARE_VERSION_INT;
    info->free_heap = esp_get_free_heap_size();
    info->min_free_heap = esp_get_minimum_free_heap_size();
    info->wifi_connected = s_device_state.wifi_connected;
    info->provisioning_mode = s_device_state.provisioning_mode;
}