#pragma once

#include <stdint.h>

#include "esp_err.h"

/** 设备的不可变硬件与固件身份信息，启动时采集一次。 */
typedef struct {
    char default_name[33];  /**< 出厂默认名，形如 kenko32-ab12 */
    char mdns_hostname[33]; /**< mDNS 主机名，始终由 MAC 派生，不随用户改名变化 */
    char mac[18];           /**< STA MAC，形如 aa:bb:cc:dd:ee:ff */
    char mac_suffix[5];
    char chip_model[16];
    char chip_revision[8]; /**< 形如 v0.2 */
    uint8_t chip_cores;
    uint32_t flash_size;
    uint32_t psram_size;
    uint32_t firmware_version;
    const char *firmware_name;
    const char *idf_version;
    const char *build_timestamp;
} device_identity_t;

esp_err_t device_info_init(void);

/** 返回启动时采集的身份信息，调用方只读。 */
const device_identity_t *device_info_identity(void);
