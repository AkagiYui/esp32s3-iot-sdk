#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

/**
 * 设备访问口令与会话。
 *
 * 这块板子没有屏幕也没有输入设备，唯一能和人交互的时刻就是配网门户，
 * 所以凭据必须在那时建立，而且必须是"人能带到别的设备上"的形式——
 * 随机令牌做不到这一点（换台手机就得手抄 32 位十六进制）。
 *
 * 设备只保存 PBKDF2-HMAC-SHA256 的派生值与盐，不保存口令本身。
 * 会话令牌只存在内存里，重启后需要重新登录。
 */

/** 会话令牌的长度（32 个十六进制字符 + 结尾 NUL）。 */
#define KENKO_AUTH_TOKEN_LEN 33

/** 口令长度限制。上限同时限制了 PBKDF2 的输入长度。 */
#define KENKO_AUTH_PASSWORD_MIN_LEN CONFIG_KENKO_AUTH_PASSWORD_MIN_LEN
#define KENKO_AUTH_PASSWORD_MAX_LEN 64

esp_err_t kenko_auth_init(void);

/** 用户是否已经设置过访问口令。未设置时设备不允许离开配网模式。 */
bool kenko_auth_is_configured(void);

/**
 * 校验口令并签发会话令牌。
 *
 * @return ESP_OK；口令错误返回 ESP_ERR_INVALID_ARG；
 *         尚未设置口令返回 ESP_ERR_INVALID_STATE；
 *         连续失败过多被临时锁定返回 ESP_ERR_NOT_ALLOWED。
 */
esp_err_t kenko_auth_login(const char *password, char *out_token, size_t out_size,
                           uint32_t *out_expires_in_seconds);

/** 校验会话令牌是否有效且未过期。 */
bool kenko_auth_validate_session(const char *token);

/** 注销单个会话；token 为 NULL 时注销全部。 */
void kenko_auth_logout(const char *token);

/**
 * 设置或修改访问口令，成功后签发一个新会话并注销其余全部会话。
 *
 * @param current 旧口令。已设置口令且调用方未经物理接近认证时必须提供；
 *                配网模式下（物理接近即信任边界）可传 NULL。
 * @return ESP_OK；旧口令不符返回 ESP_ERR_INVALID_ARG；
 *         新口令不满足策略返回 ESP_ERR_INVALID_SIZE。
 */
esp_err_t kenko_auth_set_password(const char *current, const char *next, char *out_token, size_t out_size,
                                  uint32_t *out_expires_in_seconds);

/** 校验口令是否满足策略，reason 会指向一句可直接展示的说明。 */
bool kenko_auth_check_policy(const char *password, const char **reason);

/** 清除口令与全部会话，用于恢复出厂设置。 */
esp_err_t kenko_auth_reset(void);
