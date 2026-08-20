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

set(COMPONENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components)

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

# 这三个模块直接处理不可信输入（HTTP 路径、DNS 报文），越界与未定义行为
# 靠肉眼和编译器告警都看不出来，交给 sanitizer。
# -fno-sanitize-recover=all 让 UBSan 直接中止，否则它只打印一行、测试照样"通过"。
option(KENKO_SANITIZE "Build host tests with AddressSanitizer and UndefinedBehaviorSanitizer" ON)

if(KENKO_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

function(kenko_add_host_test name component source)
    add_executable(${name} ${name}.c ${COMPONENTS_DIR}/${component}/${source})
    target_include_directories(${name} PRIVATE
        ${COMPONENTS_DIR}/${component}/include
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kenko_add_host_test(test_led_color kenko_board led_color.c)
kenko_add_host_test(test_dns_message kenko_wifi dns_message.c)
kenko_add_host_test(test_http_utils kenko_web http_utils.c)

# 随机输入测试同时链接两个解析器，价值主要来自跑在 sanitizer 下
add_executable(test_fuzz
    test_fuzz.c
    ${COMPONENTS_DIR}/kenko_wifi/dns_message.c
    ${COMPONENTS_DIR}/kenko_web/http_utils.c)
target_include_directories(test_fuzz PRIVATE
    ${COMPONENTS_DIR}/kenko_wifi/include
    ${COMPONENTS_DIR}/kenko_web/include
    ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_fuzz COMMAND test_fuzz)
