#include "http_utils.h"

#include <string.h>

typedef struct {
    const char *token;
    const char *suffix;
    int preference; /* 压缩率偏好，数值越大越优先 */
} encoding_spec_t;

static const encoding_spec_t k_encodings[HTTP_UTILS_MAX_ENCODINGS] = {
    {.token = "br", .suffix = ".br", .preference = 3},
    {.token = "zstd", .suffix = ".zst", .preference = 2},
    {.token = "gzip", .suffix = ".gz", .preference = 1},
};

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/** 百分号解码，同时截断 query / fragment。失败返回 false。 */
static bool decode_uri(const char *uri, char *out, size_t out_size)
{
    size_t written = 0;

    for (const char *cursor = uri; *cursor != '\0'; ++cursor) {
        if (*cursor == '?' || *cursor == '#') {
            break;
        }

        char decoded = *cursor;
        if (decoded == '%') {
            int high = hex_value(cursor[1]);
            int low = high < 0 ? -1 : hex_value(cursor[2]);
            if (low < 0) {
                return false;
            }
            decoded = (char)((high << 4) | low);
            cursor += 2;
        }

        /* 解码后不允许出现 NUL 或反斜杠，避免绕过后续的段校验。 */
        if (decoded == '\0' || decoded == '\\') {
            return false;
        }

        if (written + 1 >= out_size) {
            return false;
        }
        out[written++] = decoded;
    }

    out[written] = '\0';
    return true;
}

bool http_utils_sanitize_path(const char *uri, char *out, size_t out_size)
{
    if (uri == NULL || out == NULL || out_size < 2) {
        return false;
    }

    char decoded[512];
    if (!decode_uri(uri, decoded, sizeof(decoded))) {
        return false;
    }

    if (decoded[0] != '/') {
        return false;
    }

    size_t written = 0;
    const char *cursor = decoded;

    while (*cursor != '\0') {
        while (*cursor == '/') {
            ++cursor;
        }

        const char *segment = cursor;
        while (*cursor != '\0' && *cursor != '/') {
            ++cursor;
        }

        size_t length = (size_t)(cursor - segment);
        if (length == 0) {
            continue;
        }
        if (length == 1 && segment[0] == '.') {
            continue;
        }
        if (length == 2 && segment[0] == '.' && segment[1] == '.') {
            return false;
        }

        if (written + length + 1 >= out_size) {
            return false;
        }
        out[written++] = '/';
        memcpy(&out[written], segment, length);
        written += length;
    }

    if (written == 0) {
        out[written++] = '/';
    }
    out[written] = '\0';
    return true;
}

/** 去掉预压缩后缀后返回扩展名（含点），没有扩展名时返回 NULL。 */
static const char *extension_of(const char *path, char *scratch, size_t scratch_size)
{
    size_t length = strlen(path);
    if (length >= scratch_size) {
        return NULL;
    }
    memcpy(scratch, path, length + 1);

    for (size_t index = 0; index < HTTP_UTILS_MAX_ENCODINGS; ++index) {
        size_t suffix_len = strlen(k_encodings[index].suffix);
        if (length > suffix_len && strcmp(scratch + length - suffix_len, k_encodings[index].suffix) == 0) {
            length -= suffix_len;
            scratch[length] = '\0';
            break;
        }
    }

    return strrchr(scratch, '.');
}

const char *http_utils_content_type(const char *path)
{
    char scratch[288];
    const char *ext = path == NULL ? NULL : extension_of(path, scratch, sizeof(scratch));

    if (ext == NULL) {
        return "application/octet-stream";
    }

    static const struct {
        const char *ext;
        const char *type;
    } table[] = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"},
        {".mjs", "text/javascript; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".svg", "image/svg+xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".webp", "image/webp"},
        {".ico", "image/x-icon"},
        {".woff2", "font/woff2"},
        {".txt", "text/plain; charset=utf-8"},
        {".map", "application/json; charset=utf-8"},
        {".wasm", "application/wasm"},
    };

    for (size_t index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if (strcmp(ext, table[index].ext) == 0) {
            return table[index].type;
        }
    }

    return "application/octet-stream";
}

const char *http_utils_cache_control(const char *path)
{
    char scratch[288];
    const char *ext = path == NULL ? NULL : extension_of(path, scratch, sizeof(scratch));

    /* 入口文档每次都要回源校验，否则设备升级后用户会一直停在旧页面。 */
    if (ext == NULL || strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "no-cache";
    }

    return "public, max-age=604800";
}

/** 解析单个 `Accept-Encoding` 元素上的 `;q=` 参数，扫描范围不跨越逗号。 */
static int parse_quality(const char *params)
{
    const char *cursor = params;

    while (*cursor != '\0' && *cursor != ',') {
        if (*cursor != ';') {
            ++cursor;
            continue;
        }

        ++cursor;
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (to_lower(*cursor) != 'q') {
            continue;
        }

        ++cursor;
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor != '=') {
            continue;
        }

        ++cursor;
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor < '0' || *cursor > '9') {
            return 1000;
        }

        int quality = (*cursor++ - '0') * 1000;
        if (*cursor == '.') {
            ++cursor;
            int scale = 100;
            while (scale > 0 && *cursor >= '0' && *cursor <= '9') {
                quality += (*cursor++ - '0') * scale;
                scale /= 10;
            }
        }
        return quality > 1000 ? 1000 : quality;
    }

    return 1000;
}

size_t http_utils_parse_accept_encoding(const char *header, http_encoding_t *out, size_t max_out)
{
    if (out == NULL || max_out == 0) {
        return 0;
    }

    int qualities[HTTP_UTILS_MAX_ENCODINGS];
    for (size_t index = 0; index < HTTP_UTILS_MAX_ENCODINGS; ++index) {
        qualities[index] = -1;
    }
    int wildcard_quality = -1;

    const char *cursor = header;
    while (cursor != NULL && *cursor != '\0') {
        while (*cursor == ',' || *cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }

        const char *token_start = cursor;
        while (*cursor != '\0' && *cursor != ',' && *cursor != ';') {
            ++cursor;
        }

        size_t token_len = (size_t)(cursor - token_start);
        while (token_len > 0 && (token_start[token_len - 1] == ' ' || token_start[token_len - 1] == '\t')) {
            --token_len;
        }

        const char *params = cursor;
        while (*cursor != '\0' && *cursor != ',') {
            ++cursor;
        }

        if (token_len == 0 || token_len >= 16) {
            continue;
        }

        char token[16];
        for (size_t index = 0; index < token_len; ++index) {
            token[index] = to_lower(token_start[index]);
        }
        token[token_len] = '\0';

        int quality = parse_quality(params);

        if (strcmp(token, "*") == 0) {
            wildcard_quality = quality;
            continue;
        }

        for (size_t index = 0; index < HTTP_UTILS_MAX_ENCODINGS; ++index) {
            if (strcmp(token, k_encodings[index].token) == 0) {
                qualities[index] = quality;
                break;
            }
        }
    }

    if (wildcard_quality >= 0) {
        for (size_t index = 0; index < HTTP_UTILS_MAX_ENCODINGS; ++index) {
            if (qualities[index] < 0) {
                qualities[index] = wildcard_quality;
            }
        }
    }

    /* 先按 q 值降序，q 相同时按压缩率偏好降序。 */
    size_t count = 0;
    for (size_t round = 0; round < HTTP_UTILS_MAX_ENCODINGS && count < max_out; ++round) {
        int best = -1;
        for (size_t index = 0; index < HTTP_UTILS_MAX_ENCODINGS; ++index) {
            if (qualities[index] <= 0) {
                continue;
            }
            if (best < 0 || qualities[index] > qualities[best] ||
                (qualities[index] == qualities[best] &&
                 k_encodings[index].preference > k_encodings[best].preference)) {
                best = (int)index;
            }
        }
        if (best < 0) {
            break;
        }

        out[count].token = k_encodings[best].token;
        out[count].suffix = k_encodings[best].suffix;
        out[count].quality = qualities[best];
        count++;
        qualities[best] = -1;
    }

    return count;
}
