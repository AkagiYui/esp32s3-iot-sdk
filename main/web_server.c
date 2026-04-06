#include "web_server.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app_config.h"
#include "device_info.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_config_store.h"

#include "cJSON.h"

static const char *TAG = "web_server";
static httpd_handle_t s_server;

static const char *content_type_from_path(const char *path);

typedef struct {
    char ssid[33];
    int rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

static esp_err_t send_json_response(httpd_req_t *req, int status_code, const char *payload);
static esp_err_t wifi_config_get_handler(httpd_req_t *req);
static esp_err_t wifi_config_put_handler(httpd_req_t *req);
static esp_err_t wifi_scan_get_handler(httpd_req_t *req);
static esp_err_t parse_request_body(httpd_req_t *req, char *buffer, size_t buffer_size);
static const char *authmode_to_string(wifi_auth_mode_t authmode);
static int compare_scan_result(const void *left, const void *right);

static esp_err_t send_common_headers(httpd_req_t *req, const char *path, const char *encoding)
{
    httpd_resp_set_type(req, content_type_from_path(path));
    if (encoding != NULL) {
        httpd_resp_set_hdr(req, "Content-Encoding", encoding);
        httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    }
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, int status_code, const char *payload)
{
    char status[32] = {0};
    snprintf(status, sizeof(status), "%d %s", status_code,
             status_code == 200   ? "OK"
             : status_code == 400 ? "Bad Request"
             : status_code == 500 ? "Internal Server Error"
                                  : "OK");
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    if (req->method == HTTP_HEAD) {
        return httpd_resp_send(req, NULL, 0);
    }
    return httpd_resp_sendstr(req, payload);
}

static const char *strip_encoding_suffix(const char *path)
{
    static const char *suffixes[] = { ".br", ".gz", ".zst" };

    for (size_t index = 0; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        const char *suffix = suffixes[index];
        size_t path_len = strlen(path);
        size_t suffix_len = strlen(suffix);
        if (path_len > suffix_len && strcmp(path + path_len - suffix_len, suffix) == 0) {
            return suffix;
        }
    }

    return NULL;
}

static esp_err_t captive_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.6.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
    return httpd_resp_send(req, NULL, 0);
}

static const char *content_type_from_path(const char *path)
{
    char normalized_path[256] = {0};
    strlcpy(normalized_path, path, sizeof(normalized_path));

    const char *encoding_suffix = strip_encoding_suffix(normalized_path);
    if (encoding_suffix != NULL) {
        normalized_path[strlen(normalized_path) - strlen(encoding_suffix)] = '\0';
    }

    const char *ext = strrchr(normalized_path, '.');
    if (ext == NULL) {
        return "text/plain";
    }
    if (strcmp(ext, ".html") == 0) {
        return "text/html; charset=utf-8";
    }
    if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    }
    if (strcmp(ext, ".css") == 0) {
        return "text/css";
    }
    if (strcmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    }
    if (strcmp(ext, ".json") == 0) {
        return "application/json";
    }
    return "application/octet-stream";
}

static bool file_exists(const char *path)
{
    struct stat st = {0};
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *encoding)
{
    ESP_RETURN_ON_ERROR(send_common_headers(req, path, encoding), TAG, "set headers failed");
    if (req->method == HTTP_HEAD) {
        return httpd_resp_send(req, NULL, 0);
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return ESP_FAIL;
    }

    char buffer[1024];
    size_t read_bytes = 0;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        esp_err_t err = httpd_resp_send_chunk(req, buffer, read_bytes);
        if (err != ESP_OK) {
            fclose(file);
            httpd_resp_sendstr_chunk(req, NULL);
            return err;
        }
    }

    fclose(file);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t device_info_get_handler(httpd_req_t *req)
{
    device_info_t info = {0};
    device_info_snapshot(&info);

    char payload[512];
    int length = snprintf(payload, sizeof(payload),
                          "{\"device_name\":\"%s\",\"mdns_hostname\":\"%s.local\",\"firmware_version\":%u,\"firmware_name\":\"%s\",\"free_heap\":%u,\"min_free_heap\":%u,\"wifi_connected\":%s,\"provisioning_mode\":%s,\"ip_address\":\"%s\"}",
                          info.device_name,
                          info.mdns_hostname,
                          (unsigned)info.firmware_version,
                          info.firmware_name,
                          (unsigned)info.free_heap,
                          (unsigned)info.min_free_heap,
                          info.wifi_connected ? "true" : "false",
                          info.provisioning_mode ? "true" : "false",
                          info.ip_address);

    if (length <= 0 || length >= (int)sizeof(payload)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "payload too large");
    }

    httpd_resp_set_type(req, "application/json");
    if (req->method == HTTP_HEAD) {
        return httpd_resp_send(req, NULL, 0);
    }
    return httpd_resp_send(req, payload, length);
}

static esp_err_t wifi_config_get_handler(httpd_req_t *req)
{
    wifi_credential_list_t list = {0};
    ESP_RETURN_ON_ERROR(wifi_config_store_load(&list), TAG, "load wifi config failed");

    cJSON *root = cJSON_CreateArray();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
    }

    for (size_t index = 0; index < list.count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        }

        cJSON_AddStringToObject(entry, "ssid", list.items[index].ssid);
        cJSON_AddStringToObject(entry, "password", list.items[index].password);
        cJSON_AddNumberToObject(entry, "priority", (double)index);
        cJSON_AddItemToArray(root, entry);
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json encode failed");
    }

    esp_err_t err = send_json_response(req, 200, payload);
    cJSON_free(payload);
    return err;
}

static esp_err_t parse_request_body(httpd_req_t *req, char *buffer, size_t buffer_size)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= buffer_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buffer + received, req->content_len - received);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += ret;
    }

    buffer[received] = '\0';
    return ESP_OK;
}

static esp_err_t wifi_config_put_handler(httpd_req_t *req)
{
    char body[2048] = {0};
    if (parse_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json_response(req, 400, "{\"message\":\"invalid request body\"}");
    }

    cJSON *root = cJSON_Parse(body);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return send_json_response(req, 400, "{\"message\":\"body must be a json array\"}");
    }

    wifi_credential_list_t list = {0};
    size_t count = cJSON_GetArraySize(root);
    if (count > WIFI_CONFIG_MAX_ITEMS) {
        cJSON_Delete(root);
        return send_json_response(req, 400, "{\"message\":\"too many wifi configs\"}");
    }

    for (size_t index = 0; index < count; ++index) {
        cJSON *entry = cJSON_GetArrayItem(root, (int)index);
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(entry, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(entry, "password");

        if (!cJSON_IsString(ssid) || ssid->valuestring == NULL || ssid->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return send_json_response(req, 400, "{\"message\":\"ssid is required\"}");
        }

        if (strlen(ssid->valuestring) >= WIFI_CONFIG_SSID_MAX_LEN) {
            cJSON_Delete(root);
            return send_json_response(req, 400, "{\"message\":\"ssid too long\"}");
        }

        if (cJSON_IsString(password) && password->valuestring != NULL &&
            strlen(password->valuestring) >= WIFI_CONFIG_PASSWORD_MAX_LEN) {
            cJSON_Delete(root);
            return send_json_response(req, 400, "{\"message\":\"password too long\"}");
        }

        strlcpy(list.items[list.count].ssid, ssid->valuestring, sizeof(list.items[list.count].ssid));
        strlcpy(list.items[list.count].password,
                cJSON_IsString(password) && password->valuestring != NULL ? password->valuestring : "",
                sizeof(list.items[list.count].password));
        list.count++;
    }

    cJSON_Delete(root);
    ESP_RETURN_ON_ERROR(wifi_config_store_save(&list), TAG, "save wifi config failed");
    return send_json_response(req, 200, "{\"message\":\"ok\"}");
}

static const char *authmode_to_string(wifi_auth_mode_t authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3-PSK";
        default:
            return "UNKNOWN";
    }
}

static int compare_scan_result(const void *left, const void *right)
{
    const wifi_scan_result_t *a = (const wifi_scan_result_t *)left;
    const wifi_scan_result_t *b = (const wifi_scan_result_t *)right;
    return b->rssi - a->rssi;
}

static esp_err_t wifi_scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "wifi scan failed: %s", esp_err_to_name(err));
        return send_json_response(req, 500, "{\"message\":\"wifi scan failed\"}");
    }

    uint16_t ap_count = 0;
    ESP_RETURN_ON_ERROR(esp_wifi_scan_get_ap_num(&ap_count), TAG, "get ap count failed");
    if (ap_count == 0) {
        return send_json_response(req, 200, "[]");
    }

    wifi_ap_record_t ap_records[32] = {0};
    if (ap_count > 32) {
        ap_count = 32;
    }
    uint16_t record_count = ap_count;
    ESP_RETURN_ON_ERROR(esp_wifi_scan_get_ap_records(&record_count, ap_records), TAG, "get ap records failed");

    wifi_scan_result_t results[32] = {0};
    size_t result_count = 0;
    for (uint16_t index = 0; index < record_count; ++index) {
        if (ap_records[index].ssid[0] == '\0') {
            continue;
        }

        bool exists = false;
        for (size_t existing = 0; existing < result_count; ++existing) {
            if (strcmp(results[existing].ssid, (const char *)ap_records[index].ssid) == 0) {
                if (ap_records[index].rssi > results[existing].rssi) {
                    results[existing].rssi = ap_records[index].rssi;
                    results[existing].authmode = ap_records[index].authmode;
                }
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        strlcpy(results[result_count].ssid, (const char *)ap_records[index].ssid, sizeof(results[result_count].ssid));
        results[result_count].rssi = ap_records[index].rssi;
        results[result_count].authmode = ap_records[index].authmode;
        result_count++;
    }

    qsort(results, result_count, sizeof(results[0]), compare_scan_result);

    cJSON *root = cJSON_CreateArray();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
    }

    for (size_t index = 0; index < result_count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        }
        cJSON_AddStringToObject(entry, "ssid", results[index].ssid);
        cJSON_AddNumberToObject(entry, "rssi", results[index].rssi);
        cJSON_AddStringToObject(entry, "authmode", authmode_to_string(results[index].authmode));
        cJSON_AddItemToArray(root, entry);
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json encode failed");
    }

    err = send_json_response(req, 200, payload);
    cJSON_free(payload);
    return err;
}

static esp_err_t static_get_handler(httpd_req_t *req)
{
    char request_path[128] = {0};
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    strlcpy(request_path, uri, sizeof(request_path));
    char *query = strchr(request_path, '?');
    if (query != NULL) {
        *query = '\0';
    }

    char filesystem_path[256] = {0};
    snprintf(filesystem_path, sizeof(filesystem_path), "%s%s", KENKO_WEB_BASE_PATH, request_path);

    char accept_encoding[128] = {0};
    httpd_req_get_hdr_value_str(req, "Accept-Encoding", accept_encoding, sizeof(accept_encoding));

    if (accept_encoding[0] != '\0') {
        const char *cursor = accept_encoding;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == ',') {
                ++cursor;
            }
            if (*cursor == '\0') {
                break;
            }

            char token[16] = {0};
            size_t token_len = 0;
            while (*cursor != '\0' && *cursor != ',' && *cursor != ';' && token_len + 1 < sizeof(token)) {
                token[token_len++] = (char)tolower((unsigned char)*cursor++);
            }
            token[token_len] = '\0';

            const char *suffix = NULL;
            const char *encoding = NULL;
            if (strcmp(token, "br") == 0) {
                suffix = ".br";
                encoding = "br";
            } else if (strcmp(token, "gzip") == 0) {
                suffix = ".gz";
                encoding = "gzip";
            } else if (strcmp(token, "zstd") == 0) {
                suffix = ".zst";
                encoding = "zstd";
            }

            if (suffix != NULL) {
                char encoded_path[272] = {0};
                snprintf(encoded_path, sizeof(encoded_path), "%s%s", filesystem_path, suffix);
                if (file_exists(encoded_path)) {
                    ESP_LOGI(TAG, "serving encoded asset %s (%s)", encoded_path, encoding);
                    return send_file(req, encoded_path, encoding);
                }
            }

            while (*cursor != '\0' && *cursor != ',') {
                ++cursor;
            }
        }
    }

    if (!file_exists(filesystem_path)) {
        snprintf(filesystem_path, sizeof(filesystem_path), "%s/index.html", KENKO_WEB_BASE_PATH);
    }

    if (!file_exists(filesystem_path)) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
    }

    return send_file(req, filesystem_path, NULL);
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = KENKO_HTTP_PORT;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 20;
    config.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "start web server failed");

    const httpd_uri_t api_uri = {
        .uri = "/api/device-info",
        .method = HTTP_GET,
        .handler = device_info_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t api_uri_head = {
        .uri = "/api/device-info",
        .method = HTTP_HEAD,
        .handler = device_info_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_config_get_uri = {
        .uri = "/api/wifi-config",
        .method = HTTP_GET,
        .handler = wifi_config_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_config_head_uri = {
        .uri = "/api/wifi-config",
        .method = HTTP_HEAD,
        .handler = wifi_config_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_config_put_uri = {
        .uri = "/api/wifi-config",
        .method = HTTP_PUT,
        .handler = wifi_config_put_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_scan_uri = {
        .uri = "/api/wifi-scan",
        .method = HTTP_GET,
        .handler = wifi_scan_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t wifi_scan_head_uri = {
        .uri = "/api/wifi-scan",
        .method = HTTP_HEAD,
        .handler = wifi_scan_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_generate_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_generate_204_head = {
        .uri = "/generate_204",
        .method = HTTP_HEAD,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_hotspot_detect = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_hotspot_detect_head = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_HEAD,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_connecttest = {
        .uri = "/connecttest.txt",
        .method = HTTP_GET,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_connecttest_head = {
        .uri = "/connecttest.txt",
        .method = HTTP_HEAD,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_ncsi = {
        .uri = "/ncsi.txt",
        .method = HTTP_GET,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t captive_ncsi_head = {
        .uri = "/ncsi.txt",
        .method = HTTP_HEAD,
        .handler = captive_redirect_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t static_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t static_uri_head = {
        .uri = "/*",
        .method = HTTP_HEAD,
        .handler = static_get_handler,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &api_uri), TAG, "register /api/device-info GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &api_uri_head), TAG, "register /api/device-info HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_config_get_uri), TAG, "register /api/wifi-config GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_config_head_uri), TAG, "register /api/wifi-config HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_config_put_uri), TAG, "register /api/wifi-config PUT failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_scan_uri), TAG, "register /api/wifi-scan GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &wifi_scan_head_uri), TAG, "register /api/wifi-scan HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_generate_204), TAG, "register /generate_204 GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_generate_204_head), TAG, "register /generate_204 HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_hotspot_detect), TAG, "register /hotspot-detect.html GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_hotspot_detect_head), TAG, "register /hotspot-detect.html HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_connecttest), TAG, "register /connecttest.txt GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_connecttest_head), TAG, "register /connecttest.txt HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_ncsi), TAG, "register /ncsi.txt GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_ncsi_head), TAG, "register /ncsi.txt HEAD failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &static_uri), TAG, "register static GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &static_uri_head), TAG, "register static HEAD failed");
    ESP_LOGI(TAG, "web server started on port %d", config.server_port);
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}