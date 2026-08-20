#include "api_handlers.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "device_info.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "kenko_auth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kenko_core.h"
#include "ota_service.h"
#include "sdkconfig.h"
#include "settings_store.h"
#include "storage_fs.h"
#include "time_sync.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "api";

#define API_BODY_MAX_BYTES CONFIG_KENKO_API_BODY_MAX_BYTES
#define OTA_CHUNK_BYTES CONFIG_KENKO_OTA_CHUNK_BYTES

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

    if (req->content_len == 0) {
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

    static const char *labels[] = {CONFIG_KENKO_STORAGE_PARTITION, CONFIG_KENKO_WEB_PARTITION};
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

/**
 * 任务栈水位。
 *
 * 栈大小基本靠估，不采集水位的话溢出只会表现为随机 crash。
 * uxTaskGetSystemState() 一次拿到全部任务，包括 IDLE / wifi / tiT / httpd 这些
 * 不是我们创建、但同样会溢出的任务。
 */
static cJSON *build_tasks_json(void)
{
    cJSON *tasks = cJSON_CreateArray();
    if (tasks == NULL) {
        return NULL;
    }

    UBaseType_t count = uxTaskGetNumberOfTasks();
    TaskStatus_t *status = calloc(count, sizeof(TaskStatus_t));
    if (status == NULL) {
        return tasks;
    }

    count = uxTaskGetSystemState(status, count, NULL);
    for (UBaseType_t index = 0; index < count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            break;
        }
        cJSON_AddStringToObject(entry, "name", status[index].pcTaskName);
        cJSON_AddNumberToObject(entry, "priority", status[index].uxCurrentPriority);
        /* 单位是字（word），换算成字节才和 xTaskCreate 的入参可比。 */
        cJSON_AddNumberToObject(entry, "stack_free",
                                (double)status[index].usStackHighWaterMark * sizeof(StackType_t));
        cJSON_AddItemToArray(tasks, entry);
    }

    free(status);
    return tasks;
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
    cJSON_AddStringToObject(device, "state", kenko_state_name(kenko_state_get()));
    cJSON_AddBoolToObject(device, "provisioning", kenko_state_is_provisioning());
    cJSON_AddBoolToObject(device, "password_configured", kenko_auth_is_configured());
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
    /* esp_timer_get_time() 返回微秒，这里在浮点域上换算成毫秒。 */
    cJSON_AddNumberToObject(runtime, "uptime_ms", (double)esp_timer_get_time() / 1000.0);

    cJSON *heap = cJSON_CreateObject();
    cJSON_AddItemToObject(heap, "internal", build_heap_json(HEAP_CAPS_INTERNAL));
    if (identity->psram_size > 0) {
        cJSON_AddItemToObject(heap, "psram", build_heap_json(HEAP_CAPS_PSRAM));
    }
    cJSON_AddItemToObject(runtime, "heap", heap);
    cJSON_AddItemToObject(runtime, "tasks", build_tasks_json());
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

/* ---------------- 鉴权 ---------------- */

/** 定长比较，不因首个不同字节的位置而提前返回，避免时序侧信道。 */
static bool constant_time_equals(const char *left, const char *right, size_t length)
{
    uint8_t diff = 0;
    for (size_t index = 0; index < length; ++index) {
        diff |= (uint8_t)((unsigned char)left[index] ^ (unsigned char)right[index]);
    }
    return diff == 0;
}

/** 从请求头里取出会话令牌，找不到返回 NULL。 */
static const char *extract_session_token(httpd_req_t *req, char *buffer, size_t buffer_size)
{
    if (httpd_req_get_hdr_value_str(req, "Authorization", buffer, buffer_size) == ESP_OK &&
        strncasecmp(buffer, "Bearer ", 7) == 0) {
        return buffer + 7;
    }
    if (httpd_req_get_hdr_value_str(req, "X-Session-Token", buffer, buffer_size) == ESP_OK) {
        return buffer;
    }
    return NULL;
}

/**
 * 判断请求是否被授权。
 *
 * 配网模式下不校验：此时设备开着无密码热点，用户手上还没有凭据，
 * 物理接近就是这一阶段的信任边界。一旦接入局域网，所有接口都要会话令牌。
 */
static bool request_is_authorized(httpd_req_t *req)
{
    if (kenko_state_is_provisioning()) {
        return true;
    }

    if (!kenko_auth_is_configured()) {
        /* 口令没设起来就不拦，否则配置分区异常时会把用户彻底锁在外面。
         * 正常路径上设备不会走到这里：没设口令根本不允许离开配网模式。 */
        ESP_LOGW(TAG, "access password is not configured, allowing request");
        return true;
    }

    char header[128] = {0};
    const char *token = extract_session_token(req, header, sizeof(header));
    return token != NULL && kenko_auth_validate_session(token);
}

static esp_err_t send_unauthorized(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer realm=\"kenko\"");
    return send_error(req, 401, "unauthorized", "log in with the device access password");
}

/* ---------------- 登录与口令 ---------------- */

static esp_err_t send_session(httpd_req_t *req, const char *token, uint32_t expires_in)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_error(req, 500, "no_memory", "out of memory");
    }
    cJSON_AddStringToObject(root, "token", token);
    cJSON_AddNumberToObject(root, "expires_in", expires_in);
    return send_json(req, 200, root);
}

/** 未鉴权即可访问：前端要靠它判断该显示"设置口令"还是"登录"。 */
static esp_err_t auth_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        return send_method_not_allowed(req, "GET, HEAD");
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_error(req, 500, "no_memory", "out of memory");
    }

    cJSON_AddBoolToObject(root, "configured", kenko_auth_is_configured());
    cJSON_AddBoolToObject(root, "authenticated", request_is_authorized(req));
    cJSON_AddBoolToObject(root, "provisioning", kenko_state_is_provisioning());
    cJSON_AddNumberToObject(root, "password_min_length", KENKO_AUTH_PASSWORD_MIN_LEN);
    return send_json(req, 200, root);
}

static esp_err_t auth_login_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    cJSON *root = NULL;
    esp_err_t response = ESP_OK;
    if (read_json_body(req, &root, &response) != ESP_OK) {
        return response;
    }

    const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(password) || password->valuestring == NULL) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_body", "password is required");
    }

    char token[KENKO_AUTH_TOKEN_LEN] = {0};
    uint32_t expires_in = 0;
    esp_err_t err = kenko_auth_login(password->valuestring, token, sizeof(token), &expires_in);
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_STATE) {
        return send_error(req, 409, "password_not_set", "no access password has been set yet");
    }
    if (err == ESP_ERR_NOT_ALLOWED) {
        httpd_resp_set_hdr(req, "Retry-After", "1");
        return send_error(req, 429, "too_many_attempts", "too many failed attempts, try again shortly");
    }
    if (err != ESP_OK) {
        return send_error(req, 401, "invalid_password", "wrong access password");
    }

    return send_session(req, token, expires_in);
}

static esp_err_t auth_logout_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST");
    }

    char header[128] = {0};
    const char *token = extract_session_token(req, header, sizeof(header));
    kenko_auth_logout(token);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "logged_out");
    return send_json(req, 200, root);
}

static esp_err_t auth_password_handler(httpd_req_t *req)
{
    if (req->method != HTTP_PUT && req->method != HTTP_POST) {
        return send_method_not_allowed(req, "POST, PUT");
    }

    if (!storage_fs_storage_available()) {
        return send_error(req, 503, "storage_unavailable", "config partition is not mounted");
    }

    cJSON *root = NULL;
    esp_err_t response = ESP_OK;
    if (read_json_body(req, &root, &response) != ESP_OK) {
        return response;
    }

    const cJSON *next = cJSON_GetObjectItemCaseSensitive(root, "password");
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current_password");

    if (!cJSON_IsString(next) || next->valuestring == NULL) {
        cJSON_Delete(root);
        return send_error(req, 400, "invalid_body", "password is required");
    }

    const char *reason = NULL;
    if (!kenko_auth_check_policy(next->valuestring, &reason)) {
        cJSON_Delete(root);
        return send_error(req, 400, "weak_password", reason);
    }

    /*
     * 配网模式下不要求旧口令：那时用户是物理接近的，而且这正是忘记口令后的
     * 找回途径——长按 BOOT 5 秒回到配网模式，重设一个新的。
     */
    const char *current_value = NULL;
    if (!kenko_state_is_provisioning() && kenko_auth_is_configured()) {
        if (!cJSON_IsString(current) || current->valuestring == NULL) {
            cJSON_Delete(root);
            return send_error(req, 400, "current_password_required",
                              "the current password is required to change it");
        }
        current_value = current->valuestring;
    }

    char token[KENKO_AUTH_TOKEN_LEN] = {0};
    uint32_t expires_in = 0;
    esp_err_t err =
        kenko_auth_set_password(current_value, next->valuestring, token, sizeof(token), &expires_in);
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_ARG) {
        return send_error(req, 401, "invalid_password", "the current password does not match");
    }
    if (err != ESP_OK) {
        return send_error(req, 500, "persist_failed", esp_err_to_name(err));
    }

    return send_session(req, token, expires_in);
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
    kenko_event_post(KENKO_EVENT_REBOOT);
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

    kenko_event_post(KENKO_EVENT_FACTORY_RESET);
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
    if (req->content_len == 0) {
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

    size_t remaining = (size_t)req->content_len;
    while (remaining > 0) {
        size_t wanted = remaining < OTA_CHUNK_BYTES ? remaining : (size_t)OTA_CHUNK_BYTES;
        int received = httpd_req_recv(req, buffer, wanted);
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
        remaining -= (size_t)received;
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

    kenko_event_post(KENKO_EVENT_REBOOT);
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

    kenko_event_post(KENKO_EVENT_SETTINGS_CHANGED);
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
    cJSON_AddStringToObject(root, "state", kenko_state_name(kenko_state_get()));
    return send_json(req, 200, root);
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        return send_method_not_allowed(req, "GET, HEAD");
    }

    wifi_scan_entry_t entries[CONFIG_KENKO_WIFI_SCAN_MAX_RESULTS];
    size_t count = 0;
    esp_err_t err =
        wifi_manager_scan(entries, CONFIG_KENKO_WIFI_SCAN_MAX_RESULTS, &count, query_flag(req, "force"));
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

    /*
     * 没设访问口令就不许离开配网模式。配网门户是这块板子唯一能和人交互的时刻，
     * 一旦接入局域网就再没有建立凭据的安全通道了。
     */
    if (!kenko_auth_is_configured()) {
        return send_error(req, 409, "password_not_set", "set an access password before joining a network");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "connecting");
    cJSON_AddStringToObject(root, "note", "the provisioning hotspot will shut down");
    esp_err_t err = send_json(req, 202, root);

    kenko_event_post(KENKO_EVENT_APPLY_WIFI_CONFIG);
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

    kenko_event_post(KENKO_EVENT_ENTER_PROVISIONING);
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

typedef esp_err_t (*api_handler_fn)(httpd_req_t *req);

typedef struct {
    const char *uri;
    api_handler_fn handler;
    /** 登录本身和"是否需要登录"的探测必须能在未鉴权时访问，否则无法自举。 */
    bool allow_unauthenticated;
} api_route_t;

/* 顺序即匹配顺序：具体路径必须排在通配兜底之前。 */
static const api_route_t k_routes[] = {
    {"/api/auth/status", auth_status_handler, true},
    {"/api/auth/login", auth_login_handler, true},
    {"/api/auth/logout", auth_logout_handler, false},
    {"/api/auth/password", auth_password_handler, false},
    {"/api/system/info", system_info_handler, false},
    {"/api/system/reboot", system_reboot_handler, false},
    {"/api/system/factory-reset", system_factory_reset_handler, false},
    {"/api/system/ota/confirm", system_ota_confirm_handler, false},
    {"/api/system/ota", system_ota_handler, false},
    {"/api/system/revert-to-factory", system_revert_factory_handler, false},
    {"/api/system/coredump", system_coredump_handler, false},
    {"/api/settings", settings_handler, false},
    {"/api/wifi/status", wifi_status_handler, false},
    {"/api/wifi/scan", wifi_scan_handler, false},
    {"/api/wifi/config", wifi_config_handler, false},
    {"/api/wifi/connect", wifi_connect_handler, false},
    {"/api/wifi/provision", wifi_provision_handler, false},
    {"/api", api_not_found_handler, false},
    {"/api/*", api_not_found_handler, false},
};

/**
 * 所有接口的统一入口。
 *
 * esp_http_server 没有中间件，把鉴权收在这一个地方，比在十几个处理函数里
 * 各写一遍要可靠得多——漏掉一处就等于没做。
 */
static esp_err_t api_dispatch(httpd_req_t *req)
{
    const api_route_t *route = req->user_ctx;
    if (route == NULL) {
        return send_error(req, 500, "internal", "route not bound");
    }

    if (!route->allow_unauthenticated && !request_is_authorized(req)) {
        ESP_LOGW(TAG, "rejected unauthorized %s", route->uri);
        return send_unauthorized(req);
    }

    return route->handler(req);
}

esp_err_t api_handlers_register(httpd_handle_t server)
{
    for (size_t index = 0; index < sizeof(k_routes) / sizeof(k_routes[0]); ++index) {
        const httpd_uri_t uri = {
            .uri = k_routes[index].uri,
            .method = HTTP_ANY,
            .handler = api_dispatch,
            /* 静态数组，生命周期覆盖整个 server。 */
            .user_ctx = (void *)&k_routes[index],
        };

        esp_err_t err = httpd_register_uri_handler(server, &uri);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", k_routes[index].uri, esp_err_to_name(err));
            return err;
        }
    }

    return ESP_OK;
}
