#include "wifi_config_store.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app_config.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "wifi_store";

static bool is_valid_entry(const wifi_credential_t *item)
{
    return item != NULL && item->ssid[0] != '\0';
}

static esp_err_t write_json_string(const char *json)
{
    FILE *file = fopen(KENKO_WIFI_CONFIG_FILE, "w");
    if (file == NULL) {
        return ESP_FAIL;
    }

    if (fputs(json, file) == EOF) {
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);
    return ESP_OK;
}

esp_err_t wifi_config_store_load(wifi_credential_list_t *list)
{
    struct stat st = {0};
    FILE *file = NULL;
    char *buffer = NULL;
    cJSON *root = NULL;

    if (list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(list, 0, sizeof(*list));

    if (stat(KENKO_WIFI_CONFIG_FILE, &st) != 0 || st.st_size == 0) {
        ESP_LOGI(TAG, "wifi config file missing or empty");
        return ESP_OK;
    }

    file = fopen(KENKO_WIFI_CONFIG_FILE, "r");
    if (file == NULL) {
        return ESP_FAIL;
    }

    buffer = calloc(1, st.st_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    if (fread(buffer, 1, st.st_size, file) != (size_t)st.st_size) {
        free(buffer);
        fclose(file);
        return ESP_FAIL;
    }
    fclose(file);

    root = cJSON_Parse(buffer);
    free(buffer);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "wifi config is not a JSON array");
        return ESP_OK;
    }

    size_t count = cJSON_GetArraySize(root);
    for (size_t index = 0; index < count && list->count < WIFI_CONFIG_MAX_ITEMS; ++index) {
        cJSON *entry = cJSON_GetArrayItem(root, (int)index);
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(entry, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(entry, "password");
        if (!cJSON_IsString(ssid) || ssid->valuestring == NULL || ssid->valuestring[0] == '\0') {
            continue;
        }

        wifi_credential_t *target = &list->items[list->count++];
        strlcpy(target->ssid, ssid->valuestring, sizeof(target->ssid));
        if (cJSON_IsString(password) && password->valuestring != NULL) {
            strlcpy(target->password, password->valuestring, sizeof(target->password));
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "loaded %u wifi configs", (unsigned)list->count);
    return ESP_OK;
}

esp_err_t wifi_config_store_save(const wifi_credential_list_t *list)
{
    if (list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateArray();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t index = 0; index < list->count; ++index) {
        const wifi_credential_t *item = &list->items[index];
        if (!is_valid_entry(item)) {
            continue;
        }

        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddStringToObject(entry, "ssid", item->ssid);
        cJSON_AddStringToObject(entry, "password", item->password);
        cJSON_AddItemToArray(root, entry);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = write_json_string(json);
    cJSON_free(json);
    return err;
}

esp_err_t wifi_config_store_add(const char *ssid, const char *password)
{
    wifi_credential_list_t list = {0};
    ESP_RETURN_ON_ERROR(wifi_config_store_load(&list), TAG, "load failed");
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (list.count >= WIFI_CONFIG_MAX_ITEMS) {
        return ESP_ERR_NO_MEM;
    }

    strlcpy(list.items[list.count].ssid, ssid, sizeof(list.items[list.count].ssid));
    strlcpy(list.items[list.count].password, password == NULL ? "" : password, sizeof(list.items[list.count].password));
    list.count++;
    return wifi_config_store_save(&list);
}

esp_err_t wifi_config_store_remove(size_t index)
{
    wifi_credential_list_t list = {0};
    ESP_RETURN_ON_ERROR(wifi_config_store_load(&list), TAG, "load failed");
    if (index >= list.count) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t cursor = index; cursor + 1 < list.count; ++cursor) {
        list.items[cursor] = list.items[cursor + 1];
    }
    list.count--;
    return wifi_config_store_save(&list);
}

bool wifi_config_store_has_entries(void)
{
    wifi_credential_list_t list = {0};
    return wifi_config_store_load(&list) == ESP_OK && list.count > 0;
}

esp_err_t wifi_config_store_clear(void)
{
    return write_json_string("[]");
}