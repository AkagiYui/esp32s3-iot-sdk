# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(hello_world)

# ---- Web Frontend Build ----
set(EMBED_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/embed-web)

# LittleFS 暂存根目录（构建目录内），结构示例：
#   littlefs_root/
#     web/        ← embed-web/dist/ 内容（前端页面）
#     <其他子目录>  ← 其他业务文件可在此添加
set(LITTLEFS_ROOT ${CMAKE_BINARY_DIR}/littlefs_root)
set(LITTLEFS_WEB_DIR ${LITTLEFS_ROOT}/web)

# 构建前端并将产物复制到暂存目录的 web/ 子目录
add_custom_target(build_web
    COMMAND pnpm build
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LITTLEFS_WEB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${EMBED_WEB_DIR}/dist ${LITTLEFS_WEB_DIR}
    WORKING_DIRECTORY ${EMBED_WEB_DIR}
    COMMENT "Building embed web frontend with pnpm..."
    VERBATIM
)

# 将暂存根目录打包为 LittleFS 分区镜像 (build/storage.bin)
# FLASH_IN_PROJECT 使其随 idf.py flash 一起烧录
# DEPENDS build_web 确保先执行前端构建
littlefs_create_partition_image(storage ${LITTLEFS_ROOT}
    FLASH_IN_PROJECT
    DEPENDS build_web
)
