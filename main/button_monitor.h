#pragma once

#include "esp_err.h"

/**
 * 启动 BOOT 键监听。
 *
 * 长按 5 秒进入配网模式，长按 10 秒恢复出厂设置。
 * 两个动作都只投递事件，实际处理统一交给状态机，避免多处直接调 esp_restart()。
 */
esp_err_t button_monitor_start(void);
