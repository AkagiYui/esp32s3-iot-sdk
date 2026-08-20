#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

/**
 * 挂载 `storage` 与 `web` 两个 LittleFS 分区。
 *
 * 任一分区挂载失败都不会中止启动：设备仍然要能进入配网模式并把故障暴露给用户，
 * 而不是在开机时 panic 进入重启循环。
 */
esp_err_t storage_fs_init(void);

/** 配置分区是否可用；不可用时所有配置读写都会失败。 */
bool storage_fs_storage_available(void);

/** 前端资源分区是否可用；不可用时 Web 服务器只返回内置的兜底页面。 */
bool storage_fs_web_available(void);

/** 查询某个分区的容量与占用（字节）。 */
esp_err_t storage_fs_usage(const char *label, size_t *total, size_t *used);

/** 格式化配置分区，用于恢复出厂设置。 */
esp_err_t storage_fs_format_storage(void);
