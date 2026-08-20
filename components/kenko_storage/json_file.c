#include "json_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"

static const char *TAG = "json_file";

esp_err_t json_file_read(const char *path, cJSON **out_root)
{
    if (path == NULL || out_root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = NULL;

    struct stat st = {0};
    if (stat(path, &st) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (st.st_size <= 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (st.st_size > JSON_FILE_MAX_BYTES) {
        ESP_LOGW(TAG, "%s is %ld bytes, refusing to parse", path, (long)st.st_size);
        return ESP_ERR_INVALID_SIZE;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return ESP_FAIL;
    }

    char *buffer = calloc(1, (size_t)st.st_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)st.st_size, file);
    fclose(file);
    if (read_bytes != (size_t)st.st_size) {
        free(buffer);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (root == NULL) {
        ESP_LOGW(TAG, "%s is not valid json", path);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *out_root = root;
    return ESP_OK;
}

esp_err_t json_file_write(const char *path, const cJSON *root)
{
    if (path == NULL || root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *payload = cJSON_PrintUnformatted(root);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t payload_len = strlen(payload);
    if (payload_len > JSON_FILE_MAX_BYTES) {
        cJSON_free(payload);
        return ESP_ERR_INVALID_SIZE;
    }

    char temp_path[128];
    int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    if (written <= 0 || written >= (int)sizeof(temp_path)) {
        cJSON_free(payload);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = ESP_OK;
    FILE *file = fopen(temp_path, "wb");
    if (file == NULL) {
        cJSON_free(payload);
        return ESP_FAIL;
    }

    if (fwrite(payload, 1, payload_len, file) != payload_len) {
        err = ESP_FAIL;
    }
    if (fflush(file) != 0 || fsync(fileno(file)) != 0) {
        err = ESP_FAIL;
    }
    fclose(file);
    cJSON_free(payload);

    if (err != ESP_OK) {
        unlink(temp_path);
        return err;
    }

    /* LittleFS 的 rename 是原子的，掉电不会留下半截文件。 */
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        ESP_LOGE(TAG, "rename %s -> %s failed", temp_path, path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t json_file_delete(const char *path)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (unlink(path) != 0) {
        struct stat st = {0};
        if (stat(path, &st) != 0) {
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    return ESP_OK;
}
