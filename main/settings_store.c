#include "settings_store.h"

#include <string.h>

#include "app_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "json_file.h"

static const char *TAG = "settings";

static app_settings_t s_settings;
static char s_default_device_name[SETTINGS_DEVICE_NAME_MAX_LEN];
static SemaphoreHandle_t s_lock;

static void lock(void)
{
    if (s_lock != NULL) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock != NULL) {
        xSemaphoreGive(s_lock);
    }
}

static void apply_defaults(app_settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    strlcpy(settings->device_name, s_default_device_name, sizeof(settings->device_name));
    strlcpy(settings->timezone, KENKO_DEFAULT_TIMEZONE, sizeof(settings->timezone));
    settings->ntp_enabled = true;
    settings->led_brightness = 100;
}

bool settings_store_validate(const app_settings_t *settings, const char **reason)
{
    const char *ignored = NULL;
    if (reason == NULL) {
        reason = &ignored;
    }

    if (settings == NULL) {
        *reason = "settings is null";
        return false;
    }

    size_t name_len = strnlen(settings->device_name, sizeof(settings->device_name));
    if (name_len == 0 || name_len >= sizeof(settings->device_name)) {
        *reason = "device_name must be 1..32 bytes";
        return false;
    }

    for (size_t index = 0; index < name_len; ++index) {
        unsigned char c = (unsigned char)settings->device_name[index];
        if (c < 0x20 || c == 0x7f) {
            *reason = "device_name contains control characters";
            return false;
        }
    }

    size_t tz_len = strnlen(settings->timezone, sizeof(settings->timezone));
    if (tz_len == 0 || tz_len >= sizeof(settings->timezone)) {
        *reason = "timezone must be 1..38 bytes";
        return false;
    }

    if (settings->led_brightness > 100) {
        *reason = "led_brightness must be 0..100";
        return false;
    }

    return true;
}

static esp_err_t persist_locked(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "device_name", s_settings.device_name);
    cJSON_AddStringToObject(root, "timezone", s_settings.timezone);
    cJSON_AddBoolToObject(root, "ntp_enabled", s_settings.ntp_enabled);
    cJSON_AddNumberToObject(root, "led_brightness", s_settings.led_brightness);

    esp_err_t err = json_file_write(KENKO_SETTINGS_FILE, root);
    cJSON_Delete(root);
    return err;
}

esp_err_t settings_store_init(const char *default_device_name)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    strlcpy(s_default_device_name, default_device_name == NULL ? KENKO_DEVICE_PREFIX : default_device_name,
            sizeof(s_default_device_name));

    lock();
    apply_defaults(&s_settings);

    cJSON *root = NULL;
    esp_err_t err = json_file_read(KENKO_SETTINGS_FILE, &root);
    if (err == ESP_OK && cJSON_IsObject(root)) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "device_name");
        const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(root, "timezone");
        const cJSON *ntp = cJSON_GetObjectItemCaseSensitive(root, "ntp_enabled");
        const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "led_brightness");

        if (cJSON_IsString(name) && name->valuestring != NULL) {
            strlcpy(s_settings.device_name, name->valuestring, sizeof(s_settings.device_name));
        }
        if (cJSON_IsString(timezone) && timezone->valuestring != NULL) {
            strlcpy(s_settings.timezone, timezone->valuestring, sizeof(s_settings.timezone));
        }
        if (cJSON_IsBool(ntp)) {
            s_settings.ntp_enabled = cJSON_IsTrue(ntp);
        }
        if (cJSON_IsNumber(brightness)) {
            double value = brightness->valuedouble;
            s_settings.led_brightness = (uint8_t)(value < 0 ? 0 : (value > 100 ? 100 : value));
        }
    }
    cJSON_Delete(root);

    /* 文件缺失、损坏或字段越界时都回落到默认值，并立即写回一份干净的配置。 */
    if (!settings_store_validate(&s_settings, NULL)) {
        ESP_LOGW(TAG, "settings invalid, falling back to defaults");
        apply_defaults(&s_settings);
    }

    esp_err_t persist_err = persist_locked();
    app_settings_t snapshot = s_settings;
    unlock();

    if (persist_err != ESP_OK) {
        ESP_LOGW(TAG, "persist settings failed: %s", esp_err_to_name(persist_err));
    }

    ESP_LOGI(TAG, "device_name=%s timezone=%s ntp=%d brightness=%u", snapshot.device_name, snapshot.timezone,
             (int)snapshot.ntp_enabled, (unsigned)snapshot.led_brightness);
    return ESP_OK;
}

void settings_store_get(app_settings_t *out)
{
    if (out == NULL) {
        return;
    }

    lock();
    *out = s_settings;
    unlock();
}

esp_err_t settings_store_update(const app_settings_t *settings)
{
    if (!settings_store_validate(settings, NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    lock();
    s_settings = *settings;
    esp_err_t err = persist_locked();
    unlock();
    return err;
}

esp_err_t settings_store_reset(void)
{
    lock();
    apply_defaults(&s_settings);
    esp_err_t err = persist_locked();
    unlock();
    return err;
}
