#include "kenko_auth.h"

#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "json_file.h"
#include "psa/crypto.h"

static const char *TAG = "kenko_auth";

#define AUTH_FILE CONFIG_KENKO_STORAGE_BASE_PATH "/" CONFIG_KENKO_AUTH_FILE_NAME
#define SALT_LEN 16
#define KEY_LEN 32
#define HEX_LEN(bytes) ((bytes) * 2 + 1)

typedef struct {
    char token[KENKO_AUTH_TOKEN_LEN];
    int64_t expires_at_us;
} auth_session_t;

static struct {
    SemaphoreHandle_t lock;
    bool configured;
    uint8_t salt[SALT_LEN];
    uint8_t key[KEY_LEN];
    uint32_t iterations;
    auth_session_t sessions[CONFIG_KENKO_AUTH_MAX_SESSIONS];
    uint32_t failed_attempts;
    int64_t lockout_until_us;
} s_auth;

static void lock(void)
{
    xSemaphoreTake(s_auth.lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_auth.lock);
}

/* ---------------- 十六进制与常量时间比较 ---------------- */

static void to_hex(const uint8_t *data, size_t length, char *out)
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t index = 0; index < length; ++index) {
        out[index * 2] = HEX[data[index] >> 4];
        out[index * 2 + 1] = HEX[data[index] & 0x0f];
    }
    out[length * 2] = '\0';
}

static bool from_hex(const char *text, uint8_t *out, size_t length)
{
    if (text == NULL || strlen(text) != length * 2) {
        return false;
    }

    for (size_t index = 0; index < length * 2; ++index) {
        char c = text[index];
        uint8_t nibble;
        if (c >= '0' && c <= '9') {
            nibble = (uint8_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            nibble = (uint8_t)(c - 'a' + 10);
        } else {
            return false;
        }
        if (index % 2 == 0) {
            out[index / 2] = (uint8_t)(nibble << 4);
        } else {
            out[index / 2] |= nibble;
        }
    }
    return true;
}

/** 定长比较，不因首个不同字节的位置而提前返回。 */
static bool constant_time_equals(const uint8_t *left, const uint8_t *right, size_t length)
{
    uint8_t diff = 0;
    for (size_t index = 0; index < length; ++index) {
        diff |= (uint8_t)(left[index] ^ right[index]);
    }
    return diff == 0;
}

/* ---------------- 口令派生 ---------------- */

/*
 * PBKDF2-HMAC-SHA256。
 *
 * 用 PSA Crypto 而不是 mbedtls_md_hmac_*：在 ESP-IDF v6 带的 TF-PSA-Crypto 里，
 * 经典的 md HMAC 接口已经被划进 MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS，
 * PSA 才是公开且长期支持的那一套，而且它自带 PBKDF2，不用我们手写派生循环。
 */
static esp_err_t derive_key(const char *password, const uint8_t *salt, uint32_t iterations,
                            uint8_t out[KEY_LEN])
{
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    const psa_algorithm_t algorithm = PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256);
    esp_err_t err = ESP_FAIL;

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attributes, algorithm);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_PASSWORD);

    if (psa_import_key(&attributes, (const uint8_t *)password, strlen(password), &key_id) != PSA_SUCCESS) {
        goto cleanup;
    }

    if (psa_key_derivation_setup(&operation, algorithm) != PSA_SUCCESS ||
        psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST, iterations) !=
            PSA_SUCCESS ||
        psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt, SALT_LEN) !=
            PSA_SUCCESS ||
        psa_key_derivation_input_key(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD, key_id) != PSA_SUCCESS ||
        psa_key_derivation_output_bytes(&operation, out, KEY_LEN) != PSA_SUCCESS) {
        goto cleanup;
    }

    err = ESP_OK;

cleanup:
    psa_key_derivation_abort(&operation);
    if (key_id != PSA_KEY_ID_NULL) {
        psa_destroy_key(key_id);
    }
    psa_reset_key_attributes(&attributes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "key derivation failed");
    }
    return err;
}

/* ---------------- 持久化 ---------------- */

static esp_err_t persist_locked(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char salt_hex[HEX_LEN(SALT_LEN)];
    char key_hex[HEX_LEN(KEY_LEN)];
    to_hex(s_auth.salt, SALT_LEN, salt_hex);
    to_hex(s_auth.key, KEY_LEN, key_hex);

    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "kdf", "pbkdf2-hmac-sha256");
    cJSON_AddNumberToObject(root, "iterations", s_auth.iterations);
    cJSON_AddStringToObject(root, "salt", salt_hex);
    cJSON_AddStringToObject(root, "key", key_hex);

    esp_err_t err = json_file_write(AUTH_FILE, root);
    cJSON_Delete(root);
    return err;
}

static void load_locked(void)
{
    cJSON *root = NULL;
    if (json_file_read(AUTH_FILE, &root) != ESP_OK || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    const cJSON *salt = cJSON_GetObjectItemCaseSensitive(root, "salt");
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(root, "key");
    const cJSON *iterations = cJSON_GetObjectItemCaseSensitive(root, "iterations");

    if (cJSON_IsString(salt) && cJSON_IsString(key) && cJSON_IsNumber(iterations) &&
        from_hex(salt->valuestring, s_auth.salt, SALT_LEN) &&
        from_hex(key->valuestring, s_auth.key, KEY_LEN) && iterations->valuedouble >= 1) {
        s_auth.iterations = (uint32_t)iterations->valuedouble;
        s_auth.configured = true;
    } else {
        ESP_LOGW(TAG, "credential file is malformed, treating device as unconfigured");
    }

    cJSON_Delete(root);
}

/* ---------------- 会话 ---------------- */

static void clear_sessions_locked(void)
{
    memset(s_auth.sessions, 0, sizeof(s_auth.sessions));
}

/** 签发新会话，占用最早过期的槽位。 */
static void issue_session_locked(char *out_token, size_t out_size, uint32_t *out_expires_in_seconds)
{
    uint8_t raw[(KENKO_AUTH_TOKEN_LEN - 1) / 2];
    esp_fill_random(raw, sizeof(raw));

    char token[KENKO_AUTH_TOKEN_LEN];
    to_hex(raw, sizeof(raw), token);

    size_t victim = 0;
    for (size_t index = 1; index < CONFIG_KENKO_AUTH_MAX_SESSIONS; ++index) {
        if (s_auth.sessions[index].expires_at_us < s_auth.sessions[victim].expires_at_us) {
            victim = index;
        }
    }

    const int64_t ttl_us = (int64_t)CONFIG_KENKO_AUTH_SESSION_TTL_HOURS * 3600 * 1000000;
    strlcpy(s_auth.sessions[victim].token, token, sizeof(s_auth.sessions[victim].token));
    s_auth.sessions[victim].expires_at_us = esp_timer_get_time() + ttl_us;

    if (out_token != NULL) {
        strlcpy(out_token, token, out_size);
    }
    if (out_expires_in_seconds != NULL) {
        *out_expires_in_seconds = (uint32_t)(ttl_us / 1000000);
    }
}

/* ---------------- 对外接口 ---------------- */

esp_err_t kenko_auth_init(void)
{
    if (s_auth.lock == NULL) {
        s_auth.lock = xSemaphoreCreateMutex();
        if (s_auth.lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed");
        return ESP_FAIL;
    }

    lock();
    s_auth.iterations = CONFIG_KENKO_AUTH_PBKDF2_ITERATIONS;
    clear_sessions_locked();
    load_locked();
    bool configured = s_auth.configured;
    unlock();

    if (configured) {
        ESP_LOGI(TAG, "access password is set");
    } else {
        ESP_LOGW(TAG, "access password is NOT set; the device will stay in provisioning mode");
    }
    return ESP_OK;
}

bool kenko_auth_is_configured(void)
{
    lock();
    bool configured = s_auth.configured;
    unlock();
    return configured;
}

bool kenko_auth_check_policy(const char *password, const char **reason)
{
    const char *ignored = NULL;
    if (reason == NULL) {
        reason = &ignored;
    }

    if (password == NULL) {
        *reason = "password is required";
        return false;
    }

    size_t length = strnlen(password, KENKO_AUTH_PASSWORD_MAX_LEN + 1);
    if (length < KENKO_AUTH_PASSWORD_MIN_LEN) {
        *reason = "password is too short";
        return false;
    }
    if (length > KENKO_AUTH_PASSWORD_MAX_LEN) {
        *reason = "password is too long";
        return false;
    }

    for (size_t index = 0; index < length; ++index) {
        unsigned char c = (unsigned char)password[index];
        if (c < 0x20 || c == 0x7f) {
            *reason = "password contains control characters";
            return false;
        }
    }

    return true;
}

/** 校验口令；调用方需持锁。 */
static bool password_matches_locked(const char *password)
{
    uint8_t derived[KEY_LEN];
    if (derive_key(password, s_auth.salt, s_auth.iterations, derived) != ESP_OK) {
        return false;
    }

    bool matches = constant_time_equals(derived, s_auth.key, KEY_LEN);
    memset(derived, 0, sizeof(derived));
    return matches;
}

esp_err_t kenko_auth_login(const char *password, char *out_token, size_t out_size,
                           uint32_t *out_expires_in_seconds)
{
    if (password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    lock();

    if (!s_auth.configured) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }

    if (esp_timer_get_time() < s_auth.lockout_until_us) {
        unlock();
        return ESP_ERR_NOT_ALLOWED;
    }

    if (!password_matches_locked(password)) {
        /* 连续失败按次数线性拉长锁定，拖垮在线爆破。离线爆破由 PBKDF2 挡。 */
        s_auth.failed_attempts++;
        int64_t penalty_ms = (int64_t)s_auth.failed_attempts * CONFIG_KENKO_AUTH_LOCKOUT_STEP_MS;
        if (penalty_ms > CONFIG_KENKO_AUTH_LOCKOUT_MAX_MS) {
            penalty_ms = CONFIG_KENKO_AUTH_LOCKOUT_MAX_MS;
        }
        s_auth.lockout_until_us = esp_timer_get_time() + penalty_ms * 1000;
        uint32_t attempts = s_auth.failed_attempts;
        unlock();

        ESP_LOGW(TAG, "failed login attempt #%u, locked for %lld ms", (unsigned)attempts,
                 (long long)penalty_ms);
        return ESP_ERR_INVALID_ARG;
    }

    s_auth.failed_attempts = 0;
    s_auth.lockout_until_us = 0;
    issue_session_locked(out_token, out_size, out_expires_in_seconds);
    unlock();

    ESP_LOGI(TAG, "login succeeded");
    return ESP_OK;
}

bool kenko_auth_validate_session(const char *token)
{
    if (token == NULL || token[0] == '\0') {
        return false;
    }

    size_t length = strnlen(token, KENKO_AUTH_TOKEN_LEN);
    if (length != KENKO_AUTH_TOKEN_LEN - 1) {
        return false;
    }

    int64_t now = esp_timer_get_time();
    bool valid = false;

    lock();
    for (size_t index = 0; index < CONFIG_KENKO_AUTH_MAX_SESSIONS; ++index) {
        const auth_session_t *session = &s_auth.sessions[index];
        if (session->expires_at_us <= now || session->token[0] == '\0') {
            continue;
        }
        if (constant_time_equals((const uint8_t *)session->token, (const uint8_t *)token, length)) {
            valid = true;
            break;
        }
    }
    unlock();

    return valid;
}

void kenko_auth_logout(const char *token)
{
    lock();
    if (token == NULL) {
        clear_sessions_locked();
    } else {
        for (size_t index = 0; index < CONFIG_KENKO_AUTH_MAX_SESSIONS; ++index) {
            if (strcmp(s_auth.sessions[index].token, token) == 0) {
                memset(&s_auth.sessions[index], 0, sizeof(s_auth.sessions[index]));
                break;
            }
        }
    }
    unlock();
}

esp_err_t kenko_auth_set_password(const char *current, const char *next, char *out_token, size_t out_size,
                                  uint32_t *out_expires_in_seconds)
{
    if (!kenko_auth_check_policy(next, NULL)) {
        return ESP_ERR_INVALID_SIZE;
    }

    lock();

    if (s_auth.configured && current != NULL && !password_matches_locked(current)) {
        unlock();
        ESP_LOGW(TAG, "password change rejected: current password does not match");
        return ESP_ERR_INVALID_ARG;
    }

    esp_fill_random(s_auth.salt, sizeof(s_auth.salt));
    s_auth.iterations = CONFIG_KENKO_AUTH_PBKDF2_ITERATIONS;

    esp_err_t err = derive_key(next, s_auth.salt, s_auth.iterations, s_auth.key);
    if (err != ESP_OK) {
        unlock();
        return err;
    }

    s_auth.configured = true;
    s_auth.failed_attempts = 0;
    s_auth.lockout_until_us = 0;

    /* 改口令要把别处的会话全部踢掉，只保留这次新签发的。 */
    clear_sessions_locked();
    issue_session_locked(out_token, out_size, out_expires_in_seconds);

    err = persist_locked();
    unlock();

    ESP_LOGW(TAG, "access password updated (%s)", esp_err_to_name(err));
    return err;
}

esp_err_t kenko_auth_reset(void)
{
    lock();
    s_auth.configured = false;
    memset(s_auth.salt, 0, sizeof(s_auth.salt));
    memset(s_auth.key, 0, sizeof(s_auth.key));
    clear_sessions_locked();
    s_auth.failed_attempts = 0;
    s_auth.lockout_until_us = 0;
    unlock();

    esp_err_t err = json_file_delete(AUTH_FILE);
    ESP_LOGW(TAG, "access password cleared");
    return err;
}
