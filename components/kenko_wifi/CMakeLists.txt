# STA 连接循环、配网 SoftAP、凭据持久化与 captive portal 的 DNS 服务。
idf_component_register(
    SRCS
        "wifi_manager.c"
        "wifi_config_store.c"
        "dns_captive.c"
        "dns_message.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        kenko_core
    PRIV_REQUIRES
        esp_event
        esp_netif
        esp_timer
        espressif__cjson
        kenko_settings
        kenko_storage
        lwip)
