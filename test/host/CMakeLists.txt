# 固件里与 ESP-IDF 解耦的纯逻辑模块的 host 端单测。
# 用法：
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
cmake_minimum_required(VERSION 3.16)
project(kenko_host_tests C)

# 与 ESP-IDF v6 保持一致：固件侧用 -std=gnu23 编译，host 测试编的是同一批源文件，
# 标准不一致就失去了「本机先跑一遍」的意义。CMake 会按编译器版本挑 gnu23 / gnu2x。
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../main)

# -Wtype-limits 要显式写出来：GCC 的 -Wextra 已经包含它，但 clang 没有，
# 只靠本机的 clang 会漏掉恒真比较，到 CI 的 GCC 上才炸。
add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wtype-limits
)

function(kenko_add_host_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_include_directories(${name} PRIVATE ${MAIN_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kenko_add_host_test(test_led_color ${MAIN_DIR}/led_color.c)
kenko_add_host_test(test_dns_message ${MAIN_DIR}/dns_message.c)
kenko_add_host_test(test_http_utils ${MAIN_DIR}/http_utils.c)
