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

static const char *TAG = "web_server";
static httpd_handle_t s_server;

static const char *content_type_from_path(const char *path);

static esp_err_t send_common_headers(httpd_req_t *req, const char *path, const char *encoding)
{
    httpd_resp_set_type(req, content_type_from_path(path));
    if (encoding != NULL) {
        httpd_resp_set_hdr(req, "Content-Encoding", encoding);
        httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    }
    return ESP_OK;
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

    httpd_register_uri_handler(s_server, &api_uri);
    httpd_register_uri_handler(s_server, &api_uri_head);
    httpd_register_uri_handler(s_server, &captive_generate_204);
    httpd_register_uri_handler(s_server, &captive_generate_204_head);
    httpd_register_uri_handler(s_server, &captive_hotspot_detect);
    httpd_register_uri_handler(s_server, &captive_hotspot_detect_head);
    httpd_register_uri_handler(s_server, &captive_connecttest);
    httpd_register_uri_handler(s_server, &captive_connecttest_head);
    httpd_register_uri_handler(s_server, &captive_ncsi);
    httpd_register_uri_handler(s_server, &captive_ncsi_head);
    httpd_register_uri_handler(s_server, &static_uri);
    httpd_register_uri_handler(s_server, &static_uri_head);
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