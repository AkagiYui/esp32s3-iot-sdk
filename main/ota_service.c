#include "ota_service.h"

#include <string.h>

#include "esp_app_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ota";

static struct {
    SemaphoreHandle_t lock;
    esp_ota_handle_t handle;
    const esp_partition_t *target;
    ota_state_t state;
    size_t received;
    size_t total;
    char message[80];
    uint8_t header[sizeof(esp_image_header_t)];
    size_t header_len;
    bool header_checked;
} s_ota;

static void set_message(const char *message)
{
    strlcpy(s_ota.message, message == NULL ? "" : message, sizeof(s_ota.message));
}

static void lock(void)
{
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_ota.lock);
}

static void reset_locked(ota_state_t state, const char *message)
{
    if (s_ota.handle != 0) {
        esp_ota_abort(s_ota.handle);
        s_ota.handle = 0;
    }
    s_ota.state = state;
    s_ota.received = 0;
    s_ota.total = 0;
    s_ota.header_len = 0;
    s_ota.header_checked = false;
    s_ota.target = NULL;
    set_message(message);
}

esp_err_t ota_service_init(void)
{
    s_ota.lock = xSemaphoreCreateMutex();
    if (s_ota.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_ota.state = OTA_STATE_IDLE;

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t img_state = ESP_OTA_IMG_UNDEFINED;
    if (running != NULL && esp_ota_get_state_partition(running, &img_state) == ESP_OK) {
        ESP_LOGI(TAG, "running from %s, image state=%d", running->label, (int)img_state);
    }
    return ESP_OK;
}

bool ota_service_awaiting_confirm(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t img_state = ESP_OTA_IMG_UNDEFINED;

    if (running == NULL || esp_ota_get_state_partition(running, &img_state) != ESP_OK) {
        return false;
    }
    return img_state == ESP_OTA_IMG_PENDING_VERIFY;
}

size_t ota_service_max_image_size(void)
{
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    return target == NULL ? 0 : target->size;
}

esp_err_t ota_service_begin(size_t total)
{
    lock();

    if (s_ota.state == OTA_STATE_RECEIVING) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL) {
        reset_locked(OTA_STATE_FAILED, "no ota partition available");
        unlock();
        return ESP_ERR_NOT_FOUND;
    }

    if (total > 0 && total > target->size) {
        reset_locked(OTA_STATE_FAILED, "image larger than ota partition");
        unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    /* OTA_WITH_SEQUENTIAL_WRITES 边收边擦，避免一次性擦除几 MB 卡住请求。 */
    esp_err_t err = esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &s_ota.handle);
    if (err != ESP_OK) {
        reset_locked(OTA_STATE_FAILED, esp_err_to_name(err));
        unlock();
        return err;
    }

    s_ota.target = target;
    s_ota.state = OTA_STATE_RECEIVING;
    s_ota.received = 0;
    s_ota.total = total;
    s_ota.header_len = 0;
    s_ota.header_checked = false;
    set_message("receiving");

    ESP_LOGI(TAG, "ota begin -> %s (%u bytes expected)", target->label, (unsigned)total);
    unlock();
    return ESP_OK;
}

/** 校验镜像头，把明显不属于本设备的固件在写入早期就挡掉。 */
static esp_err_t validate_header_locked(void)
{
    const esp_image_header_t *header = (const esp_image_header_t *)s_ota.header;

    if (header->magic != ESP_IMAGE_HEADER_MAGIC) {
        set_message("not an esp image");
        return ESP_ERR_INVALID_ARG;
    }
    if (header->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        set_message("image built for a different chip");
        return ESP_ERR_INVALID_ARG;
    }

    s_ota.header_checked = true;
    return ESP_OK;
}

esp_err_t ota_service_write(const void *data, size_t length)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lock();

    if (s_ota.state != OTA_STATE_RECEIVING) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ota.header_checked) {
        size_t missing = sizeof(s_ota.header) - s_ota.header_len;
        size_t copy = length < missing ? length : missing;
        memcpy(s_ota.header + s_ota.header_len, data, copy);
        s_ota.header_len += copy;

        if (s_ota.header_len == sizeof(s_ota.header)) {
            esp_err_t err = validate_header_locked();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "rejecting image: %s", s_ota.message);
                char reason[sizeof(s_ota.message)];
                strlcpy(reason, s_ota.message, sizeof(reason));
                reset_locked(OTA_STATE_FAILED, reason);
                unlock();
                return err;
            }
        }
    }

    if (s_ota.target != NULL && s_ota.received + length > s_ota.target->size) {
        reset_locked(OTA_STATE_FAILED, "image larger than ota partition");
        unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = esp_ota_write(s_ota.handle, data, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota write failed: %s", esp_err_to_name(err));
        reset_locked(OTA_STATE_FAILED, esp_err_to_name(err));
        unlock();
        return err;
    }

    s_ota.received += length;
    unlock();
    return ESP_OK;
}

esp_err_t ota_service_finish(void)
{
    lock();

    if (s_ota.state != OTA_STATE_RECEIVING) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ota.header_checked) {
        reset_locked(OTA_STATE_FAILED, "image too short");
        unlock();
        return ESP_ERR_INVALID_SIZE;
    }
    if (s_ota.total > 0 && s_ota.received != s_ota.total) {
        reset_locked(OTA_STATE_FAILED, "incomplete upload");
        unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = esp_ota_end(s_ota.handle);
    s_ota.handle = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota end failed: %s", esp_err_to_name(err));
        reset_locked(OTA_STATE_FAILED,
                     err == ESP_ERR_OTA_VALIDATE_FAILED ? "image validation failed" : esp_err_to_name(err));
        unlock();
        return err;
    }

    err = esp_ota_set_boot_partition(s_ota.target);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set boot partition failed: %s", esp_err_to_name(err));
        reset_locked(OTA_STATE_FAILED, esp_err_to_name(err));
        unlock();
        return err;
    }

    size_t received = s_ota.received;
    const char *label = s_ota.target->label;
    s_ota.state = OTA_STATE_READY;
    set_message("update staged, reboot to apply");

    ESP_LOGI(TAG, "ota finished: %u bytes written to %s", (unsigned)received, label);
    unlock();
    return ESP_OK;
}

void ota_service_abort(const char *reason)
{
    lock();
    if (s_ota.state == OTA_STATE_RECEIVING) {
        ESP_LOGW(TAG, "ota aborted: %s", reason == NULL ? "unknown" : reason);
        reset_locked(OTA_STATE_FAILED, reason);
    }
    unlock();
}

void ota_service_get_status(ota_status_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    lock();
    out->state = s_ota.state;
    out->received = s_ota.received;
    out->total = s_ota.total;
    strlcpy(out->message, s_ota.message, sizeof(out->message));
    unlock();

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        strlcpy(out->running_partition, running->label, sizeof(out->running_partition));
    }
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot != NULL) {
        strlcpy(out->boot_partition, boot->label, sizeof(out->boot_partition));
    }
    out->awaiting_confirm = ota_service_awaiting_confirm();
}

esp_err_t ota_service_mark_valid(void)
{
    if (!ota_service_awaiting_confirm()) {
        return ESP_OK;
    }

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "running image confirmed, rollback cancelled");
    } else {
        ESP_LOGE(TAG, "mark app valid failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ota_service_rollback(void)
{
    ESP_LOGW(TAG, "rolling back to previous image");
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}
