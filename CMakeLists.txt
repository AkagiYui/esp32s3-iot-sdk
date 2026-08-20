cmake_minimum_required(VERSION 3.16)

# ---- 版本信息（全部来自 git，保证可复现）----
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHORT_HASH)
    set(GIT_SHORT_HASH "unknown")
endif()

# 提交数当版本号：单调递增，OTA 时可以直接比大小。
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE KENKO_FIRMWARE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT KENKO_FIRMWARE_VERSION)
    set(KENKO_FIRMWARE_VERSION 0)
endif()

# 用提交时间而不是构建时间，避免每次配置都产生不同的产物。
execute_process(
    COMMAND git log -1 --format=%cI
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE KENKO_BUILD_TIMESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT KENKO_BUILD_TIMESTAMP)
    string(TIMESTAMP KENKO_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/main/firmware_version.h.in
    ${CMAKE_BINARY_DIR}/generated/firmware_version.h
    @ONLY
)

set(PROJECT_VER "${KENKO_FIRMWARE_VERSION}-${GIT_SHORT_HASH}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# 只编译 main 及其依赖，缩短构建时间与固件体积。
idf_build_set_property(MINIMAL_BUILD ON)
project(kenko-iot-sdk)

# ---- 前端构建 ----
set(EMBED_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embed-web)

add_custom_target(build_web
    COMMAND vp build
    WORKING_DIRECTORY ${EMBED_WEB_DIR}
    COMMENT "Building embed web frontend with Vite+ (vp build)..."
    VERBATIM
)

# 把 embed-web/dist/ 打包为 web 分区镜像 (build/web.bin)。
# 只烧前端: idf.py web-flash；FLASH_IN_PROJECT 使其随 idf.py flash 一起烧录。
littlefs_create_partition_image(web ${EMBED_WEB_DIR}/dist
    FLASH_IN_PROJECT
    DEPENDS build_web
)

# ---- 合并镜像（新板初始化用）----
partition_table_get_partition_info(app_offset     "--partition-boot-default" "offset")
partition_table_get_partition_info(otadata_offset "--partition-name otadata" "offset")
partition_table_get_partition_info(web_offset     "--partition-name web"     "offset")

idf_build_get_property(python PYTHON)

set(MERGED_BIN ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-merged.bin)

add_custom_target(merged_bin
    COMMAND ${python} -m esptool
        --chip esp32s3
        merge-bin
        --output     ${MERGED_BIN}
        --flash-mode ${CONFIG_ESPTOOLPY_FLASHMODE}
        --flash-size ${CONFIG_ESPTOOLPY_FLASHSIZE}
        --flash-freq ${CONFIG_ESPTOOLPY_FLASHFREQ}
        0x0               ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
        0x8000            ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
        ${otadata_offset} ${CMAKE_BINARY_DIR}/ota_data_initial.bin
        ${app_offset}     ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        ${web_offset}     ${CMAKE_BINARY_DIR}/web.bin
    DEPENDS app bootloader partition_table littlefs_web_bin
    COMMENT "Generating merged binary: ${CMAKE_PROJECT_NAME}-merged.bin"
    VERBATIM
)
