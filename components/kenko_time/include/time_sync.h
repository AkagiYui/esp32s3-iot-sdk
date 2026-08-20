#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/** 应用设置里的时区，并做一次初始化（不联网也可调用）。 */
esp_err_t time_sync_init(void);

/** 网络就绪后启动 SNTP；未开启 NTP 时是空操作。 */
esp_err_t time_sync_start(void);

/** 断网或进入配网模式时停止 SNTP。 */
void time_sync_stop(void);

/** 切换时区，立即生效并持久化由调用方负责。 */
void time_sync_apply_timezone(const char *timezone);

bool time_sync_is_synced(void);

/** 取当前本地时间的 ISO-8601 字符串与 Unix 时间戳。 */
void time_sync_snapshot(char *iso8601, size_t size, int64_t *epoch_seconds);
