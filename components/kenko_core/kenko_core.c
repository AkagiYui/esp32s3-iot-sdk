#include "kenko_core.h"

#include <stdatomic.h>

#include "esp_log.h"

static const char *TAG = "kenko_core";

ESP_EVENT_DEFINE_BASE(KENKO_EVENT);

/* 被 HTTP 任务、状态机任务和 WiFi 事件回调并发读写，用原子量而不是 volatile：
 * volatile 在 C 里既不保证原子性，也不建立 happens-before 关系。 */
static _Atomic kenko_state_t s_state = KENKO_STATE_BOOT;

esp_err_t kenko_event_post(kenko_event_id_t event_id)
{
    esp_err_t err = esp_event_post(KENKO_EVENT, (int32_t)event_id, NULL, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "post event %d failed: %s", (int)event_id, esp_err_to_name(err));
    }
    return err;
}

void kenko_state_set(kenko_state_t state)
{
    atomic_store(&s_state, state);
}

kenko_state_t kenko_state_get(void)
{
    return atomic_load(&s_state);
}

const char *kenko_state_name(kenko_state_t state)
{
    switch (state) {
    case KENKO_STATE_BOOT:
        return "boot";
    case KENKO_STATE_PROVISIONING:
        return "provisioning";
    case KENKO_STATE_CONNECTING:
        return "connecting";
    case KENKO_STATE_ONLINE:
        return "online";
    case KENKO_STATE_OFFLINE:
        return "offline";
    default:
        return "unknown";
    }
}

bool kenko_state_is_provisioning(void)
{
    return kenko_state_get() == KENKO_STATE_PROVISIONING;
}
