#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * 注册 api 命名空间下的全部路由。
 *
 * 每个 URI 只注册一次（`HTTP_ANY`），方法分发放在处理函数内部，
 * 这样不支持的方法能正确返回 405 而不是落到静态资源的 404。
 */
esp_err_t api_handlers_register(httpd_handle_t server);
