#pragma once

#include "esp_err.h"

/** 启动内置 Web 服务（幂等）。 */
esp_err_t web_server_start(void);

/** 停止 Web 服务。 */
void web_server_stop(void);
