#include "api_handlers.h"

#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "cJSON.h"
#include "device_info.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "ota_service.h"
#include "settings_store.h"
#include "storage_fs.h"
#include "time_sync.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "api";

#define API_BODY_MAX_BYTES 4096
#define OTA_CHUNK_BYTES 2048

/* ---------------- 响应辅助 ---------------- */

static const char *status_line(int status_code)
{
    switch (status_code) {
    case 200:
        return "200 OK";
    case 202:
        return "202 Accepted";
    case 400:
        return "400 Bad Request";
    case 404:
        return "404 Not Found";
    case 405:
        return "405 Method Not Allowed";
    case 409:
        return "409 Conflict";
    case 413:
        return "413 Payload Too Large";
    case 500:
        return "500 Internal Server Error";
    case 503:
        return "503 Service Unavailable";
    default:
        return "200 OK";
    }
}

/** 发送 JSON 响应并释放 root。接口响应一律不缓存。 */
static esp_err_t send_json(httpd_req_t *req, int status_code, cJSON *root)
{
    httpd_resp_set_status(req, status_line(status_code));
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    if (root == NULL) {
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        httpd_resp_set_status(req, status_line(500));
        return httpd_resp_send(req, "{\"error\":{\"code\":\"no_memory\"}}", HTTPD_RESP_USE_STRLEN);
    }

    esp_err_t err;
    if (req->method == HTTP_HEAD) {
        err = httpd_resp_send(req, NULL, 0);
    } else {
        err = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    }
    cJSON_free(payload);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, int status_code, const char *code, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();
    if (root == NULL || error == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(error);
        return send_json(req, 500, NULL);
    }

    cJSON_AddStringToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message == NULL ? code : message);
    cJSON_AddItemToObject(root, "error", error);
    return send_json(req, status_code, root);
}

static esp_err_t send_method_not_allowed(httpd_req_t *req, const char *allow)
{
    httpd_resp_set_hdr(req, "Allow", allow);
    return send_error(req, 405, "method_not_allowed", "unsupported method for this endpoint");
}

/** 读取请求体到堆上的 NUL 结尾缓冲区，调用方负责 free。 */
static esp_err_t read_body(httpd_req_t *req, char **out)
{
    *out = NULL;

    if (req->content_len <= 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (req->content_len > API_BODY_MAX_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *buffer = calloc(1, (size_t)req->content_len + 1);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int chunk = httpd_req_recv(req, buffer + received, (size_t)(req->content_len - received));
        if (chunk == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (chunk <= 0) {
            free(buffer);
            return ESP_FAIL;
        }
        received += chunk;
    }

    buffer[received] = '\0';
    *out = buffer;
    return ESP_OK;
}

/** 解析请求体为 JSON 对象；失败时已经发过响应，调用方直接返回。 */
static esp_err_t read_json_body(httpd_req_t *req, cJSON **out, esp_err_t *response)
{
    char *body = NULL;
    esp_err_t err = read_body(req, &body);

    if (err == ESP_ERR_INVALID_SIZE) {
        *response = send_error(req, 413, "body_too_large", "request body missing or too large");
        return err;
    }
    if (err != ESP_OK) {
        *response = send_error(req, 400, "body_unreadable", "failed to read request body");
        return err;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (root == NULL) {
        *response = send_error(req, 400, "invalid_json", "request body is not valid json");
        return ESP_ERR_INVALID_ARG;
    }

    *out = root;
    return ESP_OK;
}

/** 取查询串里的布尔开关，例如 `?force=1`。 */
static bool query_flag(httpd_req_t *req, const char *key)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }

    char value[8];
    if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK) {
        return false;
    }

    return strcmp(value, "1") == 0 || strcmp(value, "true") == 0;
}

/* ---------------- /api/system/info ---------------- */

static cJSON *build_filesystem_json(void)
{
    cJSON *fs = cJSON_CreateObject();
    if (fs == NULL) {
        return NULL;
    }

    static const char *labels[] = {KENKO_STORAGE_PARTITION, KENKO_WEB_PARTITION};
    for (size_t index = 0; index < sizeof(labels) / sizeof(labels[0]); ++index) {
        size_t total = 0;
        size_t used = 0;
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            continue;
        }

        bool mounted = storage_fs_usage(labels[index], &total, &used) == ESP_OK;
        cJSON_AddBoolToObject(entry, "mounted", mounted);
        cJSON_AddNumberToObject(entry, "total", (double)total);
        cJSON_AddNumberToObject(entry, "used", (double)used);
        cJSON_AddItemToObject(fs, labels[index], entry);
    }

    return fs;
}

static cJSON *build_wifi_json(void)
{
    wifi_status_t status;
    wifi_manager_get_status(&status);

    cJSON *wifi = cJSON_CreateObject();
    if (wifi == NULL) {
        return NULL;
    }

    cJSON_AddBoolToObject(wifi, "connected", status.sta_connected);
    cJSON_AddBoolToObject(wifi, "connecting", status.connecting);
    cJSON_AddBoolToObject(wifi, "ap_active", status.ap_active);
    cJSON_AddStringToObject(wifi, "mode", status.mode);
    cJSON_AddStringToObject(wifi, "ssid", status.ssid);
    cJSON_AddNumberToObject(wifi, "rssi", status.rssi);
    cJSON_AddNumberToObject(wifi, "channel", status.channel);
    cJSON_AddStringToObject(wifi, "ip", status.ip);
    cJSON_AddStringToObject(wifi, "netmask", status.netmask);
    cJSON_AddStringToObject(wifi, "gateway", status.gateway);
    cJSON_AddStringToObject(wifi, "ap_ip", status.ap_ip);
    cJSON_AddNumberToObject(wifi, "ap_clients", status.ap_clients);
    return wifi;
}

/* 内部 RAM 与 PSRAM 必须分开统计。开了 PSRAM 之后，esp_get_free_heap_size() 返回的是
 * 两者之和（8MB 量级），拿它去和内部 RAM 的总量相减只会得到负数。 */
#define HEAP_CAPS_INTERNAL (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define HEAP_CAPS_PSRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

static cJSON *build_heap_json(uint32_t caps)
{
    cJSON *heap = cJSON_CreateObject();
    if (heap == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(heap, "free", (double)heap_caps_get_free_size(caps));
    cJSON_AddNumberToObject(heap, "total", (double)heap_caps_get_total_size(caps));
    cJSON_AddNumberToObject(heap, "min_free", (double)heap_caps_get_minimum_free_size(caps));
    /* 最大连续块比剩余总量更能说明碎片化程度。 */
    cJSON_AddNumberToObject(heap, "largest_free_block", (double)heap_caps_get_largest_free_block(caps));
    return heap;
}

static esp_err_t system_info_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        return send_method_not_allowed(req, "GET, HEAD");
    }

    const device_identity_t *identity = device_info_identity();
    app_settings_t settings;
    settings_store_get(&settings);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_error(req, 500, "no_memory", "out of memory");
    }

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", settings.device_name);
    cJSON_AddStringToObject(device, "default_name", identity->default_name);
    cJSON_AddStringToObject(device, "mdns_hostname", identity->mdns_hostname);
    cJSON_AddStringToObject(device, "mdns_domain", identity->mdns_hostname);
    cJSON_AddStringToObject(device, "mac", identity->mac);
    cJSON_AddStringToObject(device, "state", app_state_name(app_state_get()));
    cJSON_AddBoolToObject(device, "provisioning", app_state_is_provisioning());
    cJSON_AddItemToObject(root, "device", device);

    cJSON *chip = cJSON_CreateObject();
    cJSON_AddStringToObject(chip, "model", identity->chip_model);
    cJSON_AddStringToObject(chip, "revision", identity->chip_revision);
    cJSON_AddNumberToObject(chip, "cores", identity->chip_cores);
    cJSON_AddNumberToObject(chip, "flash_size", (double)identity->flash_size);
    cJSON_AddNumberToObject(chip, "psram_size", (double)identity->psram_size);
    cJSON_AddItemToObject(root, "chip", chip);

    ota_status_t ota;
    ota_service_get_status(&ota);

    cJSON *firmware = cJSON_CreateObject();
    cJSON_AddNumberToObject(firmware, "version", identity->firmware_version);
    cJSON_AddStringToObject(firmware, "name", identity->firmware_name);
    cJSON_AddStringToObject(firmware, "build_time", identity->build_timestamp);
    cJSON_AddStringToObject(firmware, "idf_version", identity->idf_version);
    cJSON_AddStringToObject(firmware, "running_partition", ota.running_partition);
    cJSON_AddBoolToObject(firmware, "awaiting_confirm", ota.awaiting_confirm);
    cJSON_AddBoolToObject(firmware, "coredump_present", ota_service_has_coredump());
    cJSON_AddItemToObject(root, "firmware", firmware);

    cJSON *runtime = cJSON_CreateObject();
    cJSON_AddNumberToObject(runtime, "uptime_ms", (double)(esp_timer_get_time() / 1000));

    cJSON *heap = cJSON_CreateObject();
    cJSON_AddItemToObject(heap, "internal", build_heap_json(HEAP_CAPS_INTERNAL));
    if (identity->psram_size > 0) {
        cJSON_AddItemToObject(heap, "psram", build_heap_json(HEAP_CAPS_PSRAM));
    }
    cJSON_AddItemToObject(runtime, "heap", heap);
    cJSON_AddItemToObject(root, "runtime", runtime);

    char timestamp[40] = {0};
    int64_t epoch = 0;
    time_sync_snapshot(timestamp, sizeof(timestamp), &epoch);

    cJSON *time_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(time_json, "synced", time_sync_is_synced());
    cJSON_AddNumberToObject(time_json, "epoch", (double)epoch);
    cJSON_AddStringToObject(time_json, "local", timestamp);
    cJSON_AddStringToObject(time_json, "timezone", settings.timezone);
    cJSON_AddItemToObject(root, "time", time_json);

    cJSON_AddItemToObject(root, "wifi", build_wifi_json());
    cJSON_AddItemToObject(root, "filesystem", build_filesystem_json());

    return send_json(req, 200, root);
}

/* ---------------- 系统操作接口 ---------------- */

static esp_err_t system_reboot_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "rebooting");
    esp_err_t err = send_json(req, 202, root);

    /* 先把响应发出去，再让状态机执行重启。 */
    app_state_post_event(APP_EVENT_REBOOT);
    return err;
}

static esp_err_t system_factory_reset_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "resetting");
    esp_err_t err = send_json(req, 202, root);

    app_state_post_event(APP_EVENT_FACTORY_RESET);
    return err;
}

static const char *ota_state_name(ota_state_t state)
{
    switch (state) {
    case OTA_STATE_RECEIVING:
        return "receiving";
    case OTA_STATE_READY:
        return "ready";
    case OTA_STATE_FAILED:
        return "failed";
    case OTA_STATE_IDLE:
    default:
        return "idle";
    }
}

static cJSON *build_ota_json(void)
{
    ota_status_t status;
    ota_service_get_status(&status);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "state", ota_state_name(status.state));
    cJSON_AddNumberToObject(root, "received", (double)status.received);
    cJSON_AddNumberToObject(root, "total", (double)status.total);
    cJSON_AddStringToObject(root, "message", status.message);
    cJSON_AddStringToObject(root, "running_partition", status.running_partition);
    cJSON_AddStringToObject(root, "boot_partition", status.boot_partition);
    cJSON_AddBoolToObject(root, "awaiting_confirm", status.awaiting_confirm);
    cJSON_AddBoolToObject(root, "factory_available", status.factory_available);
    cJSON_AddNumberToObject(root, "max_image_size", (double)ota_service_max_image_size());
    return root;
}

static esp_err_t ota_upload(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        return send_error(req, 400, "empty_body", "firmware body is empty");
    }

    esp_err_t err = ota_service_begin((size_t)req->content_len);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_error(req, 409, "ota_busy", "another update is already in progress");
    }
    if (err == ESP_ERR_INVALID_SIZE) {
        return send_error(req, 413, "image_too_large", "firmware is larger than the ota partition");
    }
    if (err != ESP_OK) {
        return send_error(req, 500, "ota_begin_failed", esp_err_to_name(err));
    }

    char *buffer = malloc(OTA_CHUNK_BYTES);
    if (buffer == NULL) {
        ota_service_abort("out of memory");
        return send_error(req, 500, "no_memory", "out of memory");
    }

    int remaining = req->content_len;
    while (remaining > 0) {
        int wanted = remaining < OTA_CHUNK_BYTES ? remaining : OTA_CHUNK_BYTES;
        int received = httpd_req_recv(req, buffer, (size_t)wanted);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            free(buffer);
            ota_service_abort("upload interrupted");
            return send_error(req, 400, "upload_interrupted", "connection closed during upload");
        }

        err = ota_service_write(buffer, (size_t)received);
        if (err != ESP_OK) {
            free(buffer);
            ota_status_t status;
            ota_service_get_status(&status);
            return send_error(req, 400, "image_rejected", status.message);
        }
        remaining -= received;
    }
    free(buffer);

    err = ota_service_finish();
    if (err != ESP_OK) {
        ota_status_t status;
        ota_service_get_status(&status);
        return send_error(req, 400, "image_rejected", status.message);
    }

    ESP_LOGI(TAG, "ota upload complete, awaiting reboot");
    return send_json(req, 200, build_ota_json());
}

static esp_err_t system_ota_handler(httpd_req_t *req)
{
    switch (req->method) {
    case HTTP_GET:
    case HTTP_HEAD:
        return send_json(req, 200, build_ota_json());
    case HTTP_POST:
        return ota_upload(req);
    default:
        return send_method_not_allowed(req, "GET, HEAD, POST");
    }
}

/**
 * 回退到出厂基线镜像。
 *
 * 这是"救砖"通道，和 /api/system/factory-reset 不是一回事：后者清用户配置、不动固件，
 * 这里换的是下次启动的固件、不动用户配置。
 */
static esp_err_t system_revert_factory_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    esp_err_t err = ota_service_revert_to_factory();
    if (err == ESP_ERR_NOT_FOUND) {
        return send_error(req, 409, "no_factory_partition", "this partition layout has no factory image");
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return send_error(req, 409, "factory_image_invalid", "the factory image failed validation");
    }
    if (err != ESP_OK) {
        return send_error(req, 500, "revert_failed", esp_err_to_name(err));
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "reverting");
    cJSON_AddStringToObject(root, "note", "rebooting into the factory image");
    esp_err_t response = send_json(req, 202, root);

    app_state_post_event(APP_EVENT_REBOOT);
    return response;
}

static esp_err_t system_ota_confirm_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    esp_err_t err = ota_service_mark_valid();
    if (err != ESP_OK) {
        return send_error(req, 500, "confirm_failed", esp_err_to_name(err));
    }
    return send_json(req, 200, build_ota_json());
}

/* ---------------- /api/settings ---------------- */

static cJSON *build_settings_json(void)
{
    app_settings_t settings;
    settings_store_get(&settings);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "device_name", settings.device_name);
    cJSON_AddStringToObject(root, "timezone", settings.timezone);
    cJSON_AddBoolToObject(root, "ntp_enabled", settings.ntp_enabled);
    cJSON_AddNumberToObject(root, "led_brightness", settings.led_brightness);
    return root;
}

static esp_err_t settings_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET || req->method == HTTP_HEAD) {
        return send_json(req, 200, build_settings_json());
    }
    if (req->method != HTTP_PUT) {
        return send_method_not_allowed(req, "GET, HEAD, PUT");
    }

    if (!storage_fs_storage_available()) {
        return send_error(req, 503, "storage_unavailable", "config partition is not mounted");
    }

    cJSON *root = NULL;
    esp_err_t response = ESP_OK;
    if (read_json_body(req, &root, &response) != ESP_OK) {
        return response;
    }

    /* 以当前设置为基线做部分更新，未出现的字段保持不变。 */
    app_settings_t settings;
    settings_store_get(&settings);

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "device_name");
    const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(root, "timezone");
    const cJSON *ntp = cJSON_GetObjectItemCaseSensitive(root, "ntp_enabled");
    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "led_brightness");

    if (name != NULL) {
        if (!cJSON_IsString(name) || name->valuestring == NULL) {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_field", "device_name must be a string");
        }
        strlcpy(settings.device_name, name->valuestring, sizeof(settings.device_name));
    }
    if (timezone != NULL) {
        if (!cJSON_IsString(timezone) || timezone->valuestring == NULL) {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_field", "timezone must be a string");
        }
        strlcpy(settings.timezone, timezone->valuestring, sizeof(settings.timezone));
    }
    if (ntp != NULL) {
        if (!cJSON_IsBool(ntp)) {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_field", "ntp_enabled must be a boolean");
        }
        settings.ntp_enabled = cJSON_IsTrue(ntp);
    }
    if (brightness != NULL) {
        if (!cJSON_IsNumber(brightness) || brightness->valuedouble < 0 || brightness->valuedouble > 100) {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_field", "led_brightness must be 0..100");
        }
        settings.led_brightness = (uint8_t)brightness->valuedouble;
    }
    cJSON_Delete(root);

    const char *reason = NULL;
    if (!settings_store_validate(&settings, &reason)) {
        return send_error(req, 400, "invalid_settings", reason);
    }

    esp_err_t err = settings_store_update(&settings);
    if (err != ESP_OK) {
        return send_error(req, 500, "persist_failed", esp_err_to_name(err));
    }

    app_state_post_event(APP_EVENT_SETTINGS_CHANGED);
    return send_json(req, 200, build_settings_json());
}

/* ---------------- WiFi 接口 ---------------- */

static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        return send_method_not_allowed(req, "GET, HEAD");
    }

    cJSON *root = build_wifi_json();
    if (root == NULL) {
        return send_error(req, 500, "no_memory", "out of memory");
    }
    cJSON_AddStringToObject(root, "state", app_state_name(app_state_get()));
    return send_json(req, 200, root);
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        return send_method_not_allowed(req, "GET, HEAD");
    }

    wifi_scan_entry_t entries[KENKO_WIFI_SCAN_MAX_RESULTS];
    size_t count = 0;
    esp_err_t err = wifi_manager_scan(entries, KENKO_WIFI_SCAN_MAX_RESULTS, &count, query_flag(req, "force"));
    if (err != ESP_OK) {
        return send_error(req, 503, "scan_failed", esp_err_to_name(err));
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (root == NULL || items == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        return send_error(req, 500, "no_memory", "out of memory");
    }

    for (size_t index = 0; index < count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            break;
        }
        cJSON_AddStringToObject(entry, "ssid", entries[index].ssid);
        cJSON_AddNumberToObject(entry, "rssi", entries[index].rssi);
        cJSON_AddNumberToObject(entry, "channel", entries[index].channel);
        cJSON_AddStringToObject(entry, "authmode", wifi_manager_authmode_name(entries[index].authmode));
        cJSON_AddBoolToObject(entry, "secured", entries[index].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(items, entry);
    }

    cJSON_AddItemToObject(root, "items", items);
    return send_json(req, 200, root);
}

static esp_err_t wifi_config_get(httpd_req_t *req)
{
    wifi_credential_list_t list;
    wifi_config_store_load(&list);

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (root == NULL || items == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        return send_error(req, 500, "no_memory", "out of memory");
    }

    for (size_t index = 0; index < list.count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            break;
        }
        cJSON_AddStringToObject(entry, "ssid", list.items[index].ssid);
        /*
         * 绝不回传明文密码：设备接入家庭网络后，任何同网段的人都能读到这个接口。
         * 前端只需要知道"这条已经有密码了"。
         */
        cJSON_AddBoolToObject(entry, "has_password", list.items[index].password[0] != '\0');
        cJSON_AddItemToArray(items, entry);
    }

    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddNumberToObject(root, "max_items", WIFI_CONFIG_MAX_ITEMS);
    return send_json(req, 200, root);
}

static esp_err_t wifi_config_put(httpd_req_t *req)
{
    if (!storage_fs_storage_available()) {
        return send_error(req, 503, "storage_unavailable", "config partition is not mounted");
    }

    cJSON *root = NULL;
    esp_err_t response = ESP_OK;
    if (read_json_body(req, &root, &response) != ESP_OK) {
        return response;
    }

    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_body", "expected an object with an \"items\" array");
    }

    int count = cJSON_GetArraySize(items);
    if (count > WIFI_CONFIG_MAX_ITEMS) {
        cJSON_Delete(root);
        return send_error(req, 400, "too_many_items", "too many wifi configs");
    }

    wifi_credential_list_t list = {0};
    for (int index = 0; index < count; ++index) {
        const cJSON *entry = cJSON_GetArrayItem(items, index);
        const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(entry, "ssid");
        const cJSON *password = cJSON_GetObjectItemCaseSensitive(entry, "password");

        if (!cJSON_IsString(ssid) || ssid->valuestring == NULL || ssid->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_ssid", "ssid is required");
        }
        if (strlen(ssid->valuestring) >= WIFI_CONFIG_SSID_MAX_LEN) {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_ssid", "ssid is longer than 32 bytes");
        }

        wifi_credential_t *target = &list.items[list.count];
        strlcpy(target->ssid, ssid->valuestring, sizeof(target->ssid));

        if (password == NULL || cJSON_IsNull(password)) {
            /* 前端不回传原密码，缺省即"沿用已保存的那一份"。 */
            if (!wifi_config_store_find_password(target->ssid, target->password, sizeof(target->password))) {
                target->password[0] = '\0';
            }
        } else if (cJSON_IsString(password) && password->valuestring != NULL) {
            size_t length = strlen(password->valuestring);
            if (length >= WIFI_CONFIG_PASSWORD_MAX_LEN) {
                cJSON_Delete(root);
                return send_error(req, 400, "invalid_password", "password is longer than 63 bytes");
            }
            if (length > 0 && length < 8) {
                cJSON_Delete(root);
                return send_error(req, 400, "invalid_password",
                                  "wpa passwords must be at least 8 characters");
            }
            strlcpy(target->password, password->valuestring, sizeof(target->password));
        } else {
            cJSON_Delete(root);
            return send_error(req, 400, "invalid_password", "password must be a string or null");
        }

        list.count++;
    }
    cJSON_Delete(root);

    esp_err_t err = wifi_config_store_save(&list);
    if (err != ESP_OK) {
        return send_error(req, 500, "persist_failed", esp_err_to_name(err));
    }

    return wifi_config_get(req);
}

static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    switch (req->method) {
    case HTTP_GET:
    case HTTP_HEAD:
        return wifi_config_get(req);
    case HTTP_PUT:
        return wifi_config_put(req);
    default:
        return send_method_not_allowed(req, "GET, HEAD, PUT");
    }
}

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }
    if (!wifi_config_store_has_entries()) {
        return send_error(req, 409, "no_wifi_config", "no wifi credentials saved");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "connecting");
    cJSON_AddStringToObject(root, "note", "the provisioning hotspot will shut down");
    esp_err_t err = send_json(req, 202, root);

    app_state_post_event(APP_EVENT_APPLY_WIFI_CONFIG);
    return err;
}

static esp_err_t wifi_provision_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "provisioning");
    esp_err_t err = send_json(req, 202, root);

    app_state_post_event(APP_EVENT_ENTER_PROVISIONING);
    return err;
}

/** 崩溃现场：有 coredump 时可以用 espcoredump.py 从设备里读出来分析，看完再擦掉。 */
static esp_err_t system_coredump_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET || req->method == HTTP_HEAD) {
        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            return send_error(req, 500, "no_memory", "out of memory");
        }
        cJSON_AddBoolToObject(root, "present", ota_service_has_coredump());
        return send_json(req, 200, root);
    }

    if (req->method != HTTP_DELETE) {
        return send_method_not_allowed(req, "GET, HEAD, DELETE");
    }

    esp_err_t err = ota_service_erase_coredump();
    if (err != ESP_OK) {
        return send_error(req, 500, "erase_failed", esp_err_to_name(err));
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "present", false);
    return send_json(req, 200, root);
}

static esp_err_t api_not_found_handler(httpd_req_t *req)
{
    return send_error(req, 404, "not_found", "unknown api endpoint");
}

/* ---------------- 注册 ---------------- */

esp_err_t api_handlers_register(httpd_handle_t server)
{
    /* 顺序即匹配顺序：具体路径必须排在通配兜底之前。 */
    static const httpd_uri_t routes[] = {
        {.uri = "/api/system/info", .method = HTTP_ANY, .handler = system_info_handler},
        {.uri = "/api/system/reboot", .method = HTTP_ANY, .handler = system_reboot_handler},
        {.uri = "/api/system/factory-reset", .method = HTTP_ANY, .handler = system_factory_reset_handler},
        {.uri = "/api/system/ota/confirm", .method = HTTP_ANY, .handler = system_ota_confirm_handler},
        {.uri = "/api/system/ota", .method = HTTP_ANY, .handler = system_ota_handler},
        {.uri = "/api/settings", .method = HTTP_ANY, .handler = settings_handler},
        {.uri = "/api/wifi/status", .method = HTTP_ANY, .handler = wifi_status_handler},
        {.uri = "/api/wifi/scan", .method = HTTP_ANY, .handler = wifi_scan_handler},
        {.uri = "/api/wifi/config", .method = HTTP_ANY, .handler = wifi_config_handler},
        {.uri = "/api/wifi/connect", .method = HTTP_ANY, .handler = wifi_connect_handler},
        {.uri = "/api/wifi/provision", .method = HTTP_ANY, .handler = wifi_provision_handler},
        {.uri = "/api/*", .method = HTTP_ANY, .handler = api_not_found_handler},
    };

    for (size_t index = 0; index < sizeof(routes) / sizeof(routes[0]); ++index) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[index]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", routes[index].uri, esp_err_to_name(err));
            return err;
        }
    }

    return ESP_OK;
}
