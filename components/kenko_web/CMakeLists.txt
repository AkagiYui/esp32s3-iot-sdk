# 静态资源服务与 REST 接口。
idf_component_register(
    SRCS
        "web_server.c"
        "http_utils.c"
        "api_handlers.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        esp_app_format
        esp_http_server
        esp_hw_support
        esp_timer
        espressif__cjson
        kenko_auth
        kenko_board
        kenko_core
        kenko_ota
        kenko_settings
        kenko_storage
        kenko_time
        kenko_wifi)
