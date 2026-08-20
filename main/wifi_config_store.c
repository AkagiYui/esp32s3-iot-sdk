#include "wifi_config_store.h"

#include <string.h>

#include "app_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "json_file.h"

static const char *TAG = "wifi_store";

static wifi_credential_list_t s_cache;
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

static esp_err_t persist_locked(void)
{
    cJSON *root = cJSON_CreateArray();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t index = 0; index < s_cache.count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddStringToObject(entry, "ssid", s_cache.items[index].ssid);
        cJSON_AddStringToObject(entry, "password", s_cache.items[index].password);
        cJSON_AddItemToArray(root, entry);
    }

    esp_err_t err = json_file_write(KENKO_WIFI_CONFIG_FILE, root);
    cJSON_Delete(root);
    return err;
}

esp_err_t wifi_config_store_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    lock();
    memset(&s_cache, 0, sizeof(s_cache));

    cJSON *root = NULL;
    esp_err_t err = json_file_read(KENKO_WIFI_CONFIG_FILE, &root);
    if (err == ESP_OK && cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        for (int index = 0; index < count && s_cache.count < WIFI_CONFIG_MAX_ITEMS; ++index) {
            const cJSON *entry = cJSON_GetArrayItem(root, index);
            const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(entry, "ssid");
            const cJSON *password = cJSON_GetObjectItemCaseSensitive(entry, "password");

            if (!cJSON_IsString(ssid) || ssid->valuestring == NULL || ssid->valuestring[0] == '\0') {
                continue;
            }

            wifi_credential_t *target = &s_cache.items[s_cache.count++];
            strlcpy(target->ssid, ssid->valuestring, sizeof(target->ssid));
            if (cJSON_IsString(password) && password->valuestring != NULL) {
                strlcpy(target->password, password->valuestring, sizeof(target->password));
            }
        }
    } else if (err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "wifi config unreadable (%s), starting empty", esp_err_to_name(err));
    }
    cJSON_Delete(root);

    size_t count = s_cache.count;
    unlock();

    ESP_LOGI(TAG, "loaded %u wifi configs", (unsigned)count);
    return ESP_OK;
}

void wifi_config_store_load(wifi_credential_list_t *list)
{
    if (list == NULL) {
        return;
    }

    lock();
    *list = s_cache;
    unlock();
}

esp_err_t wifi_config_store_save(const wifi_credential_list_t *list)
{
    if (list == NULL || list->count > WIFI_CONFIG_MAX_ITEMS) {
        return ESP_ERR_INVALID_ARG;
    }

    lock();
    memset(&s_cache, 0, sizeof(s_cache));
    for (size_t index = 0; index < list->count; ++index) {
        if (list->items[index].ssid[0] == '\0') {
            continue;
        }
        s_cache.items[s_cache.count++] = list->items[index];
    }
    esp_err_t err = persist_locked();
    size_t count = s_cache.count;
    unlock();

    ESP_LOGI(TAG, "saved %u wifi configs (%s)", (unsigned)count, esp_err_to_name(err));
    return err;
}

bool wifi_config_store_has_entries(void)
{
    lock();
    bool has_entries = s_cache.count > 0;
    unlock();
    return has_entries;
}

esp_err_t wifi_config_store_clear(void)
{
    lock();
    memset(&s_cache, 0, sizeof(s_cache));
    esp_err_t err = persist_locked();
    unlock();

    ESP_LOGW(TAG, "wifi configs cleared");
    return err;
}

bool wifi_config_store_find_password(const char *ssid, char *out, size_t out_size)
{
    if (ssid == NULL || out == NULL || out_size == 0) {
        return false;
    }

    bool found = false;
    lock();
    for (size_t index = 0; index < s_cache.count; ++index) {
        if (strcmp(s_cache.items[index].ssid, ssid) == 0) {
            strlcpy(out, s_cache.items[index].password, out_size);
            found = true;
            break;
        }
    }
    unlock();
    return found;
}
