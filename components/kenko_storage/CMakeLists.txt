# LittleFS 挂载与配置文件的原子读写。
idf_component_register(
    SRCS
        "storage_fs.c"
        "json_file.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__cjson
    PRIV_REQUIRES joltwallet__littlefs)
