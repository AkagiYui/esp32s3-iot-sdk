#include "storage_fs.h"

#include "app_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_littlefs.h"

static const char *TAG = "storage_fs";

static esp_err_t mount_partition(const char *label, const char *base_path, bool format_if_mount_failed)
{
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = base_path,
        .partition_label = label,
        .partition = NULL,
        .format_if_mount_failed = format_if_mount_failed,
        .dont_mount = false,
        .grow_on_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to mount %s at %s: %s", label, base_path, esp_err_to_name(err));
        return err;
    }

    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "%s already mounted at %s", label, base_path);
        return ESP_OK;
    }

    size_t total = 0;
    size_t used = 0;
    esp_littlefs_info(label, &total, &used);
    ESP_LOGI(TAG, "mounted %s at %s total=%u used=%u", label, base_path, (unsigned)total, (unsigned)used);
    return ESP_OK;
}

esp_err_t storage_fs_init(void)
{
    ESP_ERROR_CHECK(mount_partition("storage", KENKO_STORAGE_BASE_PATH, true));
    ESP_ERROR_CHECK(mount_partition("web", KENKO_WEB_BASE_PATH, false));
    return ESP_OK;
}