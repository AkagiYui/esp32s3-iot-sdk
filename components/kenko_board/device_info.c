#include "device_info.h"

#include <stdio.h>
#include <string.h>

#include "firmware_version.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_psram.h"
#include "esp_system.h"

static const char *TAG = "device_info";

static device_identity_t s_identity;

static void fill_chip_model(esp_chip_model_t model)
{
    const char *name;

    switch (model) {
    case CHIP_ESP32:
        name = "ESP32";
        break;
    case CHIP_ESP32S2:
        name = "ESP32-S2";
        break;
    case CHIP_ESP32S3:
        name = "ESP32-S3";
        break;
    case CHIP_ESP32C3:
        name = "ESP32-C3";
        break;
    case CHIP_ESP32C6:
        name = "ESP32-C6";
        break;
    case CHIP_ESP32H2:
        name = "ESP32-H2";
        break;
    default:
        name = "unknown";
        break;
    }

    strlcpy(s_identity.chip_model, name, sizeof(s_identity.chip_model));
}

esp_err_t device_info_init(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read mac failed: %s", esp_err_to_name(err));
        return err;
    }

    snprintf(s_identity.mac, sizeof(s_identity.mac), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    snprintf(s_identity.mac_suffix, sizeof(s_identity.mac_suffix), "%02x%02x", mac[4], mac[5]);
    snprintf(s_identity.default_name, sizeof(s_identity.default_name), "%s-%s", CONFIG_KENKO_DEVICE_PREFIX,
             s_identity.mac_suffix);
    strlcpy(s_identity.mdns_hostname, s_identity.default_name, sizeof(s_identity.mdns_hostname));

    esp_chip_info_t chip_info = {0};
    esp_chip_info(&chip_info);
    fill_chip_model(chip_info.model);
    s_identity.chip_cores = chip_info.cores;
    snprintf(s_identity.chip_revision, sizeof(s_identity.chip_revision), "v%d.%d", chip_info.revision / 100,
             chip_info.revision % 100);

    uint32_t flash_size = 0;
    if (esp_flash_get_physical_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0;
    }
    s_identity.flash_size = flash_size;

#if CONFIG_SPIRAM
    s_identity.psram_size = (uint32_t)esp_psram_get_size();
#else
    s_identity.psram_size = 0;
#endif

    s_identity.firmware_version = KENKO_FIRMWARE_VERSION;
    s_identity.firmware_name = KENKO_FIRMWARE_NAME;
    s_identity.idf_version = esp_get_idf_version();
    s_identity.build_timestamp = KENKO_FIRMWARE_BUILD_TIMESTAMP;

    ESP_LOGI(TAG, "%s chip=%s %s cores=%u flash=%uMB psram=%uMB idf=%s fw=%u/%s", s_identity.default_name,
             s_identity.chip_model, s_identity.chip_revision, (unsigned)s_identity.chip_cores,
             (unsigned)(s_identity.flash_size / (1024 * 1024)),
             (unsigned)(s_identity.psram_size / (1024 * 1024)), s_identity.idf_version,
             (unsigned)s_identity.firmware_version, s_identity.firmware_name);
    return ESP_OK;
}

const device_identity_t *device_info_identity(void)
{
    return &s_identity;
}
