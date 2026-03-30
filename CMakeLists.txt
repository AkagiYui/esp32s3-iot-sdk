# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

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
