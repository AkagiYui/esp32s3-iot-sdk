#pragma once

#include "esp_err.h"

/**
 * 启动应用状态机。
 *
 * 状态机订阅 KENKO_EVENT 事件基，把事件转投到自己的队列里由独立任务处理——
 * 事件回调运行在默认事件循环任务上，而这里的处理动作包含等待任务退出、
 * 格式化分区、延迟重启这类耗时操作，直接在回调里做会把 WiFi/IP 事件一起堵住。
 *
 * 当前状态通过 kenko_state_get() 读取，组件不需要反向依赖本模块。
 */
esp_err_t app_state_start(void);
