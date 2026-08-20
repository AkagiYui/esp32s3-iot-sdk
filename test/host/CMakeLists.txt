# 固件里与 ESP-IDF 解耦的纯逻辑模块的 host 端单测。
# 用法：
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
cmake_minimum_required(VERSION 3.16)
project(kenko_host_tests C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../main)

add_compile_options(-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion)

function(kenko_add_host_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_include_directories(${name} PRIVATE ${MAIN_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kenko_add_host_test(test_led_color ${MAIN_DIR}/led_color.c)
kenko_add_host_test(test_dns_message ${MAIN_DIR}/dns_message.c)
kenko_add_host_test(test_http_utils ${MAIN_DIR}/http_utils.c)
