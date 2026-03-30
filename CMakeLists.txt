# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(hello_world)

# ---- Web Frontend Build ----
set(EMBED_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embed-web)

# 每次构建时执行 pnpm build，将 embed-web 项目打包到 embed-web/dist/
add_custom_target(build_web
    COMMAND pnpm build
    WORKING_DIRECTORY ${EMBED_WEB_DIR}
    COMMENT "Building embed web frontend with pnpm..."
    VERBATIM
)

# 将 embed-web/dist/ 打包为 LittleFS 分区镜像 (build/storage.bin)
# FLASH_IN_PROJECT 使其随 idf.py flash 一起烧录
# DEPENDS build_web 确保先执行前端构建
littlefs_create_partition_image(storage ${EMBED_WEB_DIR}/dist
    FLASH_IN_PROJECT
    DEPENDS build_web
)
