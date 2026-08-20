# 访问口令的派生、校验与会话管理。
idf_component_register(
    SRCS "kenko_auth.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_hw_support esp_timer espressif__cjson kenko_storage mbedtls)
