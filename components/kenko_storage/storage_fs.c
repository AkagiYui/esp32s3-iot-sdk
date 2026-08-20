#include "storage_fs.h"

#include "sdkconfig.h"
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "storage_fs";

static bool s_storage_available;
static bool s_web_available;

static bool mount_partition(const char *label, const char *base_path, bool format_if_mount_failed)
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
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "%s already mounted at %s", label, base_path);
        return true;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount %s at %s failed: %s", label, base_path, esp_err_to_name(err));
        return false;
    }

    size_t total = 0;
    size_t used = 0;
    esp_littlefs_info(label, &total, &used);
    ESP_LOGI(TAG, "mounted %s at %s total=%u used=%u", label, base_path, (unsigned)total, (unsigned)used);
    return true;
}

esp_err_t storage_fs_init(void)
{
    /* 配置分区可以随时重建，挂不上就格式化。 */
    s_storage_available =
        mount_partition(CONFIG_KENKO_STORAGE_PARTITION, CONFIG_KENKO_STORAGE_BASE_PATH, true);

    /* 前端资源分区由构建产物写入，不能随便格式化，否则会把页面抹掉。 */
    s_web_available = mount_partition(CONFIG_KENKO_WEB_PARTITION, CONFIG_KENKO_WEB_BASE_PATH, false);

    if (!s_storage_available) {
        ESP_LOGE(TAG, "storage partition unavailable, settings will not persist");
    }
    if (!s_web_available) {
        ESP_LOGW(TAG, "web partition unavailable, serving built-in fallback page only");
    }

    return ESP_OK;
}

bool storage_fs_storage_available(void)
{
    return s_storage_available;
}

bool storage_fs_web_available(void)
{
    return s_web_available;
}

esp_err_t storage_fs_usage(const char *label, size_t *total, size_t *used)
{
    if (label == NULL || total == NULL || used == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *total = 0;
    *used = 0;
    return esp_littlefs_info(label, total, used);
}

esp_err_t storage_fs_format_storage(void)
{
    ESP_LOGW(TAG, "formatting %s partition", CONFIG_KENKO_STORAGE_PARTITION);
    esp_err_t err = esp_littlefs_format(CONFIG_KENKO_STORAGE_PARTITION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "format failed: %s", esp_err_to_name(err));
    }
    return err;
}
