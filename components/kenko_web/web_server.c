#include "web_server.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "api_handlers.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "http_utils.h"
#include "kenko_core.h"
#include "sdkconfig.h"
#include "storage_fs.h"
#include "wifi_manager.h"

static const char *TAG = "web_server";

#define WEB_FILE_CHUNK_BYTES 1024
#define WEB_PATH_MAX 192
#define WEB_FS_PATH_MAX (WEB_PATH_MAX + sizeof(CONFIG_KENKO_WEB_BASE_PATH) + 8)

static httpd_handle_t s_server;

/* 前端分区不可用时的兜底页面，保证设备至少能告诉用户发生了什么。 */
static const char k_fallback_page[] =
    "<!doctype html><html lang=\"zh\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Web 资源未安装</title></head><body style=\"font-family:system-ui;padding:2rem;line-height:1.6\">"
    "<h1>Web 资源未安装</h1>"
    "<p>设备固件正常运行，但 <code>web</code> 分区没有挂载或没有内容。</p>"
    "<p>请烧录 <code>web.bin</code>，或直接烧录合并镜像后重试。</p>"
    "<p>设备接口仍然可用，例如 <a href=\"/api/system/info\">/api/system/info</a>。</p>"
    "</body></html>";

/** 各系统的联网探测地址，命中后要么引导到配网页，要么明确告知"网络通畅"。 */
static bool is_captive_probe(const char *path)
{
    static const char *probes[] = {
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/library/test/success.html",
        "/connecttest.txt",
        "/ncsi.txt",
        "/redirect",
        "/success.txt",
        "/canonical.html",
    };

    for (size_t index = 0; index < sizeof(probes) / sizeof(probes[0]); ++index) {
        if (strcmp(path, probes[index]) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t handle_captive_probe(httpd_req_t *req)
{
    if (kenko_state_is_provisioning()) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", KENKO_AP_URL);
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        return httpd_resp_send(req, NULL, 0);
    }

    /* 已经联网时如实回答探测，否则系统会一直提示"需要登录"。 */
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, NULL, 0);
}

static bool file_exists(const char *path, struct stat *out)
{
    struct stat st = {0};
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    if (out != NULL) {
        *out = st;
    }
    return true;
}

static void build_etag(const struct stat *st, char *out, size_t out_size)
{
    snprintf(out, out_size, "\"%lx-%lx\"", (unsigned long)st->st_size, (unsigned long)st->st_mtime);
}

static bool etag_matches(httpd_req_t *req, const char *etag)
{
    char header[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", header, sizeof(header)) != ESP_OK) {
        return false;
    }
    return strstr(header, etag) != NULL;
}

static esp_err_t send_file(httpd_req_t *req, const char *fs_path, const char *encoding, const struct stat *st,
                           const char *logical_path)
{
    char etag[48];
    build_etag(st, etag, sizeof(etag));

    httpd_resp_set_type(req, http_utils_content_type(logical_path));
    httpd_resp_set_hdr(req, "Cache-Control", http_utils_cache_control(logical_path));
    httpd_resp_set_hdr(req, "ETag", etag);
    httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    if (encoding != NULL) {
        httpd_resp_set_hdr(req, "Content-Encoding", encoding);
    }

    if (etag_matches(req, etag)) {
        httpd_resp_set_status(req, "304 Not Modified");
        return httpd_resp_send(req, NULL, 0);
    }

    if (req->method == HTTP_HEAD) {
        return httpd_resp_send(req, NULL, 0);
    }

    FILE *file = fopen(fs_path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "open %s failed", fs_path);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "cannot open asset");
    }

    char buffer[WEB_FILE_CHUNK_BYTES];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, (ssize_t)read_bytes) != ESP_OK) {
            fclose(file);
            /* 客户端提前断开时不再尝试补发终止块。 */
            return ESP_FAIL;
        }
    }

    fclose(file);
    return httpd_resp_send_chunk(req, NULL, 0);
}

/**
 * 按 `Accept-Encoding` 找出最合适的一份资源。
 * 找到预压缩版本就返回它，否则回落到原始文件。
 */
static bool resolve_asset(httpd_req_t *req, const char *logical_path, char *fs_path, size_t fs_path_size,
                          const char **encoding, struct stat *st)
{
    char base_path[WEB_FS_PATH_MAX];
    int written = snprintf(base_path, sizeof(base_path), "%s%s", CONFIG_KENKO_WEB_BASE_PATH, logical_path);
    if (written <= 0 || written >= (int)sizeof(base_path)) {
        return false;
    }

    char accept_encoding[128] = {0};
    httpd_req_get_hdr_value_str(req, "Accept-Encoding", accept_encoding, sizeof(accept_encoding));

    http_encoding_t candidates[HTTP_UTILS_MAX_ENCODINGS];
    size_t candidate_count =
        http_utils_parse_accept_encoding(accept_encoding, candidates, HTTP_UTILS_MAX_ENCODINGS);

    for (size_t index = 0; index < candidate_count; ++index) {
        char encoded_path[WEB_FS_PATH_MAX];
        written = snprintf(encoded_path, sizeof(encoded_path), "%s%s", base_path, candidates[index].suffix);
        if (written <= 0 || written >= (int)sizeof(encoded_path)) {
            continue;
        }
        if (file_exists(encoded_path, st)) {
            strlcpy(fs_path, encoded_path, fs_path_size);
            *encoding = candidates[index].token;
            return true;
        }
    }

    if (file_exists(base_path, st)) {
        strlcpy(fs_path, base_path, fs_path_size);
        *encoding = NULL;
        return true;
    }

    return false;
}

static esp_err_t send_fallback_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (req->method == HTTP_HEAD) {
        return httpd_resp_send(req, NULL, 0);
    }
    return httpd_resp_send(req, k_fallback_page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t static_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET && req->method != HTTP_HEAD) {
        httpd_resp_set_hdr(req, "Allow", "GET, HEAD");
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "unsupported method");
    }

    char path[WEB_PATH_MAX];
    if (!http_utils_sanitize_path(req->uri, path, sizeof(path))) {
        ESP_LOGW(TAG, "rejected uri: %s", req->uri);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid path");
    }

    if (is_captive_probe(path)) {
        return handle_captive_probe(req);
    }

    if (!storage_fs_web_available()) {
        return send_fallback_page(req);
    }

    if (strcmp(path, "/") == 0) {
        strlcpy(path, "/index.html", sizeof(path));
    }

    char fs_path[WEB_FS_PATH_MAX];
    const char *encoding = NULL;
    struct stat st = {0};

    if (resolve_asset(req, path, fs_path, sizeof(fs_path), &encoding, &st)) {
        return send_file(req, fs_path, encoding, &st, path);
    }

    /*
     * 单页应用回落：任何未知路径都交给入口文档，并且同样走一遍编码协商，
     * 否则回落路径会白白发送未压缩的整包。
     */
    if (resolve_asset(req, "/index.html", fs_path, sizeof(fs_path), &encoding, &st)) {
        return send_file(req, fs_path, encoding, &st, "/index.html");
    }

    return send_fallback_page(req);
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_KENKO_HTTP_PORT;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = CONFIG_KENKO_HTTPD_TASK_STACK;
    config.max_open_sockets = CONFIG_KENKO_HTTPD_MAX_SOCKETS;
    config.max_uri_handlers = CONFIG_KENKO_HTTPD_MAX_URI_HANDLERS;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "start web server failed");

    esp_err_t err = api_handlers_register(s_server);
    if (err != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
        return err;
    }

    /* 通配的静态资源处理器必须最后注册，否则会抢在接口路由前面。 */
    const httpd_uri_t static_uri = {
        .uri = "/*",
        .method = HTTP_ANY,
        .handler = static_handler,
        .user_ctx = NULL,
    };
    err = httpd_register_uri_handler(s_server, &static_uri);
    if (err != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
        return err;
    }

    ESP_LOGI(TAG, "web server listening on port %d", config.server_port);
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server == NULL) {
        return;
    }

    httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "web server stopped");
}
