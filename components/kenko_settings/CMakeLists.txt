# 用户可修改的设备设置，持久化在配置分区。
idf_component_register(
    SRCS "settings_store.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__cjson esp_hw_support kenko_storage)
