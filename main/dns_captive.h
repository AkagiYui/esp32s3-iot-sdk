#pragma once

#include <stdbool.h>

#include "esp_err.h"

/** 启动 captive portal 的 DNS 服务，把所有 A 查询指向 SoftAP 自身。 */
esp_err_t dns_captive_start(void);

/** 停止 DNS 服务，并等待服务任务真正退出后再释放 socket。 */
void dns_captive_stop(void);

bool dns_captive_is_running(void);
