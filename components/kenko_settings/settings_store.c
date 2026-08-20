#include "settings_store.h"

#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "json_file.h"
#include "sdkconfig.h"

static const char *TAG = "settings";

#define SETTINGS_FILE CONFIG_KENKO_STORAGE_BASE_PATH "/" CONFIG_KENKO_SETTINGS_FILE_NAME

static app_settings_t s_settings;
static char s_api_token[SETTINGS_API_TOKEN_LEN];
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

/** 生成 128 位随机令牌的十六进制串。 */
static void generate_api_token(char *out, size_t out_size)
{
    static const char HEX[] = "0123456789abcdef";
    uint8_t raw[(SETTINGS_API_TOKEN_LEN - 1) / 2];

    esp_fill_random(raw, sizeof(raw));
    size_t written = 0;
    for (size_t index = 0; index < sizeof(raw) && written + 2 < out_size; ++index) {
        out[written++] = HEX[raw[index] >> 4];
        out[written++] = HEX[raw[index] & 0x0f];
    }
    out[written] = '\0';
}

static bool token_is_valid(const char *token)
{
    if (strnlen(token, SETTINGS_API_TOKEN_LEN) != SETTINGS_API_TOKEN_LEN - 1) {
        return false;
    }
    for (size_t index = 0; index < SETTINGS_API_TOKEN_LEN - 1; ++index) {
        char c = token[index];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex) {
            return false;
        }
    }
    return true;
}

static void apply_defaults(app_settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    strlcpy(settings->device_name, s_default_device_name, sizeof(settings->device_name));
    strlcpy(settings->timezone, CONFIG_KENKO_DEFAULT_TIMEZONE, sizeof(settings->timezone));
    settings->ntp_enabled = true;
    settings->led_brightness = CONFIG_KENKO_SETTINGS_DEFAULT_BRIGHTNESS;
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
    cJSON_AddStringToObject(root, "api_token", s_api_token);

    esp_err_t err = json_file_write(SETTINGS_FILE, root);
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

    strlcpy(s_default_device_name, default_device_name == NULL ? "kenko" : default_device_name,
            sizeof(s_default_device_name));

    lock();
    apply_defaults(&s_settings);

    cJSON *root = NULL;
    esp_err_t err = json_file_read(SETTINGS_FILE, &root);
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

        const cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "api_token");
        if (cJSON_IsString(token) && token->valuestring != NULL) {
            strlcpy(s_api_token, token->valuestring, sizeof(s_api_token));
        }
    }
    cJSON_Delete(root);

    /* 文件缺失、损坏或字段越界时都回落到默认值，并立即写回一份干净的配置。 */
    if (!settings_store_validate(&s_settings, NULL)) {
        ESP_LOGW(TAG, "settings invalid, falling back to defaults");
        apply_defaults(&s_settings);
    }

    /* 首次启动或令牌损坏时重新生成。 */
    if (!token_is_valid(s_api_token)) {
        generate_api_token(s_api_token, sizeof(s_api_token));
        ESP_LOGW(TAG, "generated a new api token");
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

void settings_store_get_api_token(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }

    lock();
    strlcpy(out, s_api_token, out_size);
    unlock();
}

esp_err_t settings_store_rotate_api_token(char *out, size_t out_size)
{
    lock();
    generate_api_token(s_api_token, sizeof(s_api_token));
    esp_err_t err = persist_locked();
    if (out != NULL && out_size > 0) {
        strlcpy(out, s_api_token, out_size);
    }
    unlock();

    ESP_LOGW(TAG, "api token rotated");
    return err;
}
