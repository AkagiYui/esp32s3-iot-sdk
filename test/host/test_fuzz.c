/*
 * 对两个直接处理不可信输入的解析器做随机输入测试。
 *
 * 单独看断言，这个文件只检查了很弱的性质；真正的价值在于它跑在 ASan/UBSan 下——
 * 越界读、未初始化读、有符号溢出会当场中止，而这些恰恰是手写解析器最容易犯的错。
 * 随机数用固定种子的 LCG，失败可以完全复现。
 */

#include <string.h>

#include "dns_message.h"
#include "http_utils.h"
#include "test_support.h"

static uint32_t g_seed = 0x12345678u;

static uint32_t next_random(void)
{
    /* Numerical Recipes 的 LCG，够随机且完全可复现。 */
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed;
}

static size_t random_below(size_t bound)
{
    return bound == 0 ? 0 : (size_t)(next_random() % (uint32_t)bound);
}

static void test_dns_survives_random_input(void)
{
    uint8_t query[600];
    uint8_t response[700];

    for (int round = 0; round < 20000; ++round) {
        size_t length = random_below(sizeof(query) + 1);
        for (size_t index = 0; index < length; ++index) {
            query[index] = (uint8_t)next_random();
        }

        /* 输出缓冲区大小也随机，覆盖"放不下"的分支。 */
        size_t out_size = random_below(sizeof(response) + 1);
        size_t written = dns_message_build_response(query, length, 0x0106A8C0u, 60, response, out_size);

        CHECK(written <= out_size);
        if (written > 0) {
            /* 应答一定是 QR=1，而且长度至少覆盖报文头。 */
            CHECK(written >= 12);
            CHECK((response[2] & 0x80) != 0);
        }
    }
}

static void test_dns_rejects_truncated_valid_prefix(void)
{
    /* 拿一个结构合法的查询，逐字节截断，任何一处都不该越界。 */
    static const uint8_t valid[] = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 3,    'w',  'w',  'w',  7,
        'e',  'x',  'a',  'm',  'p',  'l',  'e',  3,    'c',  'o',  'm',  0,    0x00, 0x01, 0x00, 0x01,
    };
    uint8_t response[128];

    for (size_t length = 0; length <= sizeof(valid); ++length) {
        size_t written =
            dns_message_build_response(valid, length, 0x0106A8C0u, 60, response, sizeof(response));
        if (length < sizeof(valid)) {
            CHECK_INT(written, 0);
        } else {
            CHECK(written > 0);
        }
    }
}

/** 消毒后的路径必须以 '/' 开头，且不含任何 ".." 段。 */
static bool path_is_safe(const char *path)
{
    if (path[0] != '/') {
        return false;
    }

    const char *cursor = path;
    while (*cursor != '\0') {
        while (*cursor == '/') {
            ++cursor;
        }
        const char *segment = cursor;
        while (*cursor != '\0' && *cursor != '/') {
            ++cursor;
        }
        if ((size_t)(cursor - segment) == 2 && segment[0] == '.' && segment[1] == '.') {
            return false;
        }
    }
    return true;
}

static void test_sanitize_never_yields_traversal(void)
{
    /* 只用真实 URI 里会出现的字符，让随机输入更容易命中解析分支。 */
    static const char alphabet[] = "/.%2eabAB0189?#\\ +-_~";
    char uri[64];
    char out[80];

    for (int round = 0; round < 40000; ++round) {
        size_t length = random_below(sizeof(uri) - 1);
        for (size_t index = 0; index < length; ++index) {
            uri[index] = alphabet[random_below(sizeof(alphabet) - 1)];
        }
        uri[length] = '\0';

        /* 输出缓冲区大小也随机，覆盖"放不下"的分支。 */
        size_t out_size = random_below(sizeof(out) + 1);
        if (out_size == 0) {
            continue;
        }

        if (http_utils_sanitize_path(uri, out, out_size)) {
            CHECK(strlen(out) < out_size);
            CHECK(path_is_safe(out));
        }
    }
}

static void test_content_type_survives_random_paths(void)
{
    static const char alphabet[] = "/.abzABZ089_-brgzst";
    char path[48];

    for (int round = 0; round < 10000; ++round) {
        size_t length = random_below(sizeof(path) - 1);
        for (size_t index = 0; index < length; ++index) {
            path[index] = alphabet[random_below(sizeof(alphabet) - 1)];
        }
        path[length] = '\0';

        CHECK(http_utils_content_type(path) != NULL);
        CHECK(http_utils_cache_control(path) != NULL);
    }
}

static void test_accept_encoding_survives_random_headers(void)
{
    static const char alphabet[] = "brgzipzstd, ;q=0.19*identity\t";
    char header[96];
    http_encoding_t encodings[HTTP_UTILS_MAX_ENCODINGS];

    for (int round = 0; round < 20000; ++round) {
        size_t length = random_below(sizeof(header) - 1);
        for (size_t index = 0; index < length; ++index) {
            header[index] = alphabet[random_below(sizeof(alphabet) - 1)];
        }
        header[length] = '\0';

        size_t count = http_utils_parse_accept_encoding(header, encodings, HTTP_UTILS_MAX_ENCODINGS);
        CHECK(count <= HTTP_UTILS_MAX_ENCODINGS);
        for (size_t index = 0; index < count; ++index) {
            CHECK(encodings[index].token != NULL);
            CHECK(encodings[index].suffix != NULL);
            CHECK(encodings[index].quality > 0);
            CHECK(encodings[index].quality <= 1000);
        }
    }
}

int main(void)
{
    RUN(test_dns_survives_random_input);
    RUN(test_dns_rejects_truncated_valid_prefix);
    RUN(test_sanitize_never_yields_traversal);
    RUN(test_content_type_survives_random_paths);
    RUN(test_accept_encoding_survives_random_headers);
    TEST_MAIN_END();
}
