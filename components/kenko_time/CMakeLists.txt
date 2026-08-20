# SNTP 校时与时区设置。
idf_component_register(
    SRCS "time_sync.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_netif kenko_settings lwip)
