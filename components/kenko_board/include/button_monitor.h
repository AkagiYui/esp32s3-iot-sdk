#pragma once

#include "esp_err.h"

/**
 * 启动 BOOT 键监听。
 *
 * 长按 5 秒进入配网模式，长按 10 秒恢复出厂设置。
 * 两个动作都只投递 KENKO_EVENT 事件，实际处理交给应用层的状态机，
 * 避免板级代码直接调 esp_restart()。
 */
esp_err_t button_monitor_start(void);
