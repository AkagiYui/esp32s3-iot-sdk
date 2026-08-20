#pragma once

/* 极简断言框架：host 端纯逻辑单测不值得引入外部依赖。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_test_failures;
static int g_test_checks;

#define CHECK(condition)                                                           \
    do {                                                                           \
        g_test_checks++;                                                           \
        if (!(condition)) {                                                        \
            g_test_failures++;                                                     \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                          \
    } while (0)

#define CHECK_INT(actual, expected)                                                                       \
    do {                                                                                                  \
        g_test_checks++;                                                                                  \
        long long a_ = (long long)(actual);                                                               \
        long long e_ = (long long)(expected);                                                             \
        if (a_ != e_) {                                                                                   \
            g_test_failures++;                                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #actual, a_, \
                    e_);                                                                                  \
        }                                                                                                 \
    } while (0)

#define CHECK_STR(actual, expected)                                                                       \
    do {                                                                                                  \
        g_test_checks++;                                                                                  \
        const char *a_ = (actual);                                                                        \
        const char *e_ = (expected);                                                                      \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                                                          \
            g_test_failures++;                                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #actual, \
                    a_ == NULL ? "(null)" : a_, e_);                                                      \
        }                                                                                                 \
    } while (0)

#define RUN(test_fn)                \
    do {                            \
        printf("- %s\n", #test_fn); \
        test_fn();                  \
    } while (0)

#define TEST_MAIN_END()                                                     \
    do {                                                                    \
        printf("%d checks, %d failures\n", g_test_checks, g_test_failures); \
        return g_test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;          \
    } while (0)
