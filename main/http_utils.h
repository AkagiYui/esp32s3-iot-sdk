#pragma once

#include <stdbool.h>
#include <stddef.h>

/** 支持的预压缩编码数量（br / zstd / gzip）。 */
#define HTTP_UTILS_MAX_ENCODINGS 3

typedef struct {
    const char *token;  /**< `Content-Encoding` 头里的值，例如 "br" */
    const char *suffix; /**< 预压缩文件后缀，例如 ".br" */
    int quality;        /**< 客户端给出的 q 值，放大 1000 倍 */
} http_encoding_t;

/**
 * 把请求 URI 规整成可以安全拼接到文件系统的路径。
 *
 * 会剥掉 query / fragment、做百分号解码、拒绝 `..`、反斜杠、内嵌 NUL
 * 与非法转义，并折叠重复的 `/`。结果始终以 `/` 开头。
 *
 * @return 成功返回 true；URI 非法或缓冲区不足返回 false。
 */
bool http_utils_sanitize_path(const char *uri, char *out, size_t out_size);

/** 由文件路径推导 `Content-Type`，会自动忽略预压缩后缀。 */
const char *http_utils_content_type(const char *path);

/** 由文件路径推导 `Cache-Control`：入口 HTML 必须回源，其余静态资源可长缓存。 */
const char *http_utils_cache_control(const char *path);

/**
 * 解析 `Accept-Encoding`，按客户端 q 值与压缩率偏好排序后输出候选编码。
 *
 * @param header   请求头的值，可为 NULL 或空串
 * @param out      输出数组
 * @param max_out  输出数组容量
 * @return 写入的候选数量。
 */
size_t http_utils_parse_accept_encoding(const char *header, http_encoding_t *out, size_t max_out);
