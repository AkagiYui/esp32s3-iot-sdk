#include "http_utils.h"
#include "test_support.h"

static const char *sanitize(const char *uri)
{
    static char out[256];
    if (!http_utils_sanitize_path(uri, out, sizeof(out))) {
        return NULL;
    }
    return out;
}

static void test_sanitize_normal_paths(void)
{
    CHECK_STR(sanitize("/"), "/");
    CHECK_STR(sanitize("/index.html"), "/index.html");
    CHECK_STR(sanitize("/assets/app.js"), "/assets/app.js");
    CHECK_STR(sanitize("/index.html?foo=bar"), "/index.html");
    CHECK_STR(sanitize("/index.html#hash"), "/index.html");
    CHECK_STR(sanitize("//a///b//"), "/a/b");
    CHECK_STR(sanitize("/./a/./b"), "/a/b");
}

static void test_sanitize_percent_decoding(void)
{
    CHECK_STR(sanitize("/a%20b.txt"), "/a b.txt");
    CHECK_STR(sanitize("/%2Findex.html"), "/index.html");

    /* 非法或截断的转义必须拒绝，而不是当成字面量。 */
    CHECK(sanitize("/a%2") == NULL);
    CHECK(sanitize("/a%zz") == NULL);
    CHECK(sanitize("/a%00b") == NULL);
}

static void test_sanitize_rejects_traversal(void)
{
    CHECK(sanitize("/../storage/wifi_config.json") == NULL);
    CHECK(sanitize("/a/../../b") == NULL);
    CHECK(sanitize("/%2e%2e/storage") == NULL);
    CHECK(sanitize("/%2E%2E%2Fstorage") == NULL);
    CHECK(sanitize("/a/..") == NULL);
    CHECK(sanitize("/a\\b") == NULL);
    CHECK(sanitize("relative") == NULL);
    CHECK(sanitize("") == NULL);
}

static void test_sanitize_respects_buffer_size(void)
{
    char small[8];
    CHECK(!http_utils_sanitize_path("/a-rather-long-path.html", small, sizeof(small)));
    CHECK(http_utils_sanitize_path("/ok", small, sizeof(small)));
    CHECK_STR(small, "/ok");
}

static void test_content_type(void)
{
    CHECK_STR(http_utils_content_type("/index.html"), "text/html; charset=utf-8");
    CHECK_STR(http_utils_content_type("/index.html.br"), "text/html; charset=utf-8");
    CHECK_STR(http_utils_content_type("/index.html.gz"), "text/html; charset=utf-8");
    CHECK_STR(http_utils_content_type("/index.html.zst"), "text/html; charset=utf-8");
    CHECK_STR(http_utils_content_type("/app.js"), "text/javascript; charset=utf-8");
    CHECK_STR(http_utils_content_type("/style.css"), "text/css; charset=utf-8");
    CHECK_STR(http_utils_content_type("/favicon.svg"), "image/svg+xml");
    CHECK_STR(http_utils_content_type("/favicon.ico"), "image/x-icon");
    CHECK_STR(http_utils_content_type("/font.woff2"), "font/woff2");
    CHECK_STR(http_utils_content_type("/data.json"), "application/json; charset=utf-8");
    CHECK_STR(http_utils_content_type("/noextension"), "application/octet-stream");
    CHECK_STR(http_utils_content_type("/thing.unknown"), "application/octet-stream");
}

static void test_cache_control(void)
{
    CHECK_STR(http_utils_cache_control("/index.html"), "no-cache");
    CHECK_STR(http_utils_cache_control("/index.html.br"), "no-cache");
    CHECK_STR(http_utils_cache_control("/favicon.svg"), "public, max-age=604800");
    CHECK_STR(http_utils_cache_control("/noextension"), "no-cache");
}

static void test_accept_encoding_order(void)
{
    http_encoding_t encodings[HTTP_UTILS_MAX_ENCODINGS];

    size_t count =
        http_utils_parse_accept_encoding("gzip, deflate, br, zstd", encodings, HTTP_UTILS_MAX_ENCODINGS);
    CHECK_INT(count, 3);
    CHECK_STR(encodings[0].token, "br");
    CHECK_STR(encodings[0].suffix, ".br");
    CHECK_STR(encodings[1].token, "zstd");
    CHECK_STR(encodings[2].token, "gzip");

    /* q 值优先于我们自己的压缩率偏好。 */
    count = http_utils_parse_accept_encoding("br;q=0.1, gzip;q=0.9", encodings, HTTP_UTILS_MAX_ENCODINGS);
    CHECK_INT(count, 2);
    CHECK_STR(encodings[0].token, "gzip");
    CHECK_INT(encodings[0].quality, 900);
    CHECK_STR(encodings[1].token, "br");
    CHECK_INT(encodings[1].quality, 100);
}

static void test_accept_encoding_edge_cases(void)
{
    http_encoding_t encodings[HTTP_UTILS_MAX_ENCODINGS];

    CHECK_INT(http_utils_parse_accept_encoding(NULL, encodings, HTTP_UTILS_MAX_ENCODINGS), 0);
    CHECK_INT(http_utils_parse_accept_encoding("", encodings, HTTP_UTILS_MAX_ENCODINGS), 0);
    CHECK_INT(http_utils_parse_accept_encoding("identity", encodings, HTTP_UTILS_MAX_ENCODINGS), 0);
    CHECK_INT(
        http_utils_parse_accept_encoding("deflate, identity;q=0.5", encodings, HTTP_UTILS_MAX_ENCODINGS), 0);

    /* q=0 表示明确拒绝。 */
    CHECK_INT(http_utils_parse_accept_encoding("br;q=0", encodings, HTTP_UTILS_MAX_ENCODINGS), 0);
    CHECK_INT(http_utils_parse_accept_encoding("br;q=0.000", encodings, HTTP_UTILS_MAX_ENCODINGS), 0);

    /* 大小写与多余空白 */
    size_t count = http_utils_parse_accept_encoding("  GZIP ;  q=1.0  ", encodings, HTTP_UTILS_MAX_ENCODINGS);
    CHECK_INT(count, 1);
    CHECK_STR(encodings[0].token, "gzip");

    /* 通配符补齐未显式列出的编码 */
    count = http_utils_parse_accept_encoding("gzip;q=0.5, *;q=0.2", encodings, HTTP_UTILS_MAX_ENCODINGS);
    CHECK_INT(count, 3);
    CHECK_STR(encodings[0].token, "gzip");
    CHECK_STR(encodings[1].token, "br");
    CHECK_STR(encodings[2].token, "zstd");

    /* 输出容量限制 */
    count = http_utils_parse_accept_encoding("br, gzip, zstd", encodings, 1);
    CHECK_INT(count, 1);
    CHECK_STR(encodings[0].token, "br");
}

int main(void)
{
    RUN(test_sanitize_normal_paths);
    RUN(test_sanitize_percent_decoding);
    RUN(test_sanitize_rejects_traversal);
    RUN(test_sanitize_respects_buffer_size);
    RUN(test_content_type);
    RUN(test_cache_control);
    RUN(test_accept_encoding_order);
    RUN(test_accept_encoding_edge_cases);
    TEST_MAIN_END();
}
