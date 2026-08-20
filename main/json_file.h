#pragma once

#include "cJSON.h"
#include "esp_err.h"

/** 单个配置文件允许的最大字节数，防止损坏的文件把堆吃光。 */
#define JSON_FILE_MAX_BYTES 8192

/**
 * 读取并解析 JSON 文件。
 *
 * @param path     文件路径
 * @param out_root 解析结果，调用方负责 cJSON_Delete；失败时置 NULL
 * @return ESP_OK；文件不存在返回 ESP_ERR_NOT_FOUND；内容非法返回 ESP_ERR_INVALID_RESPONSE。
 */
esp_err_t json_file_read(const char *path, cJSON **out_root);

/**
 * 原子写入 JSON 文件：先写同目录下的临时文件，再 rename 覆盖。
 * 掉电时要么是旧内容，要么是新内容，不会留下半截文件。
 */
esp_err_t json_file_write(const char *path, const cJSON *root);

/** 删除文件；文件本就不存在时同样返回 ESP_OK。 */
esp_err_t json_file_delete(const char *path);
