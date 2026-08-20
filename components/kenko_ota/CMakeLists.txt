# OTA 写入与校验、出厂基线回退、崩溃现场读写。
idf_component_register(
    SRCS "ota_service.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_update bootloader_support esp_partition espcoredump)
