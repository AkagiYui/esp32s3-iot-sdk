#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_RECEIVING,
    OTA_STATE_READY, /**< 已写完并校验通过，重启即生效 */
    OTA_STATE_FAILED,
} ota_state_t;

typedef struct {
    ota_state_t state;
    size_t received;
    size_t total;
    char message[80];
    char running_partition[17];
    char boot_partition[17];
    bool awaiting_confirm; /**< 当前运行的镜像处于待确认状态，未确认会在下次重启时回滚 */
} ota_status_t;

esp_err_t ota_service_init(void);

/** 开始一次升级，`total` 为 0 表示长度未知。 */
esp_err_t ota_service_begin(size_t total);

/** 写入一段固件数据；第一段会校验镜像头，不匹配的固件会被立刻拒绝。 */
esp_err_t ota_service_write(const void *data, size_t length);

/** 收尾并把新分区设为启动分区。 */
esp_err_t ota_service_finish(void);

/** 中止本次升级并记录原因。 */
void ota_service_abort(const char *reason);

void ota_service_get_status(ota_status_t *out);

/** 确认当前镜像可用，取消回滚。 */
esp_err_t ota_service_mark_valid(void);

/** 主动回滚到上一个可用镜像并重启。 */
esp_err_t ota_service_rollback(void);

/** 当前镜像是否处于待确认状态。 */
bool ota_service_awaiting_confirm(void);

/** 允许的最大固件大小（目标分区容量）。 */
size_t ota_service_max_image_size(void);
