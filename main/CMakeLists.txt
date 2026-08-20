# 应用层：启动序列与状态机。功能实现全部在 components/ 下，可被别的项目单独复用。
idf_component_register(
    SRCS
        "app_main.c"
        "app_state.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_event
        espressif__mdns
        kenko_board
        kenko_core
        kenko_ota
        kenko_settings
        kenko_storage
        kenko_time
        kenko_web
        kenko_wifi
        nvs_flash)
