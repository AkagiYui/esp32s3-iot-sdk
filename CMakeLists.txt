# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_SHORT_HASH)
    set(GIT_SHORT_HASH unknown)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/main/firmware_version.h.in
    ${CMAKE_BINARY_DIR}/generated/firmware_version.h
    @ONLY
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(hello_world)

# ---- Web Frontend Build ----
set(EMBED_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embed-web)

# 构建前端，产物在 embed-web/dist/
add_custom_target(build_web
    COMMAND pnpm build
    WORKING_DIRECTORY ${EMBED_WEB_DIR}
    COMMENT "Building embed web frontend with pnpm..."
    VERBATIM
)

# 将 embed-web/dist/ 单独打包为 web 分区镜像 (build/web.bin)
# 只烧前端: idf.py web-flash
# FLASH_IN_PROJECT 使其随 idf.py flash 一起烧录
littlefs_create_partition_image(web ${EMBED_WEB_DIR}/dist
    FLASH_IN_PROJECT
    DEPENDS build_web
)

# ---- Merged Binary（新板初始化用）----
# 从分区表动态读取各分区偏移，避免硬编码
partition_table_get_partition_info(factory_offset "--partition-name factory" "offset")
partition_table_get_partition_info(otadata_offset "--partition-name otadata" "offset")
partition_table_get_partition_info(web_offset     "--partition-name web"     "offset")

idf_build_get_property(python PYTHON)

set(MERGED_BIN ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-merged.bin)

# 将 bootloader + 分区表 + otadata + 固件 + web 合并为单个 bin
# 用法: cmake --build build --target merged_bin
add_custom_target(merged_bin
    COMMAND ${python} -m esptool
        --chip esp32s3
        merge_bin
        --output     ${MERGED_BIN}
        --flash_mode ${CONFIG_ESPTOOLPY_FLASHMODE}
        --flash_size ${CONFIG_ESPTOOLPY_FLASHSIZE}
        --flash_freq ${CONFIG_ESPTOOLPY_FLASHFREQ}
        0x0               ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
        0x8000            ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
        ${otadata_offset} ${CMAKE_BINARY_DIR}/ota_data_initial.bin
        ${factory_offset} ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        ${web_offset}     ${CMAKE_BINARY_DIR}/web.bin
    DEPENDS app bootloader partition_table littlefs_web_bin
    COMMENT "Generating merged binary: ${CMAKE_PROJECT_NAME}-merged.bin"
    VERBATIM
)
