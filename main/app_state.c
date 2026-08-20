#include "app_state.h"

#include <string.h>

#include "device_info.h"
#include "dns_captive.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "kenko_auth.h"
#include "kenko_core.h"
#include "mdns.h"
#include "ota_service.h"
#include "sdkconfig.h"
#include "settings_store.h"
#include "status_led.h"
#include "storage_fs.h"
#include "time_sync.h"
#include "web_server.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "app_state";

#define APP_EVENT_QUEUE_LENGTH 8

static struct {
    QueueHandle_t queue;
    bool mdns_running;
    bool storage_degraded;
} s_ctx;

static void stop_mdns(void)
{
    if (!s_ctx.mdns_running) {
        return;
    }
    mdns_free();
    s_ctx.mdns_running = false;
    ESP_LOGI(TAG, "mDNS stopped");
}

static void start_mdns(void)
{
    if (s_ctx.mdns_running) {
        return;
    }

    const device_identity_t *identity = device_info_identity();
    app_settings_t settings;
    settings_store_get(&settings);

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns init failed: %s", esp_err_to_name(err));
        return;
    }

    if (mdns_hostname_set(identity->mdns_hostname) != ESP_OK ||
        mdns_instance_name_set(settings.device_name) != ESP_OK) {
        ESP_LOGW(TAG, "mdns naming failed");
        mdns_free();
        return;
    }

    /* 公告 HTTP 服务，配套 App 或局域网扫描工具可以直接发现设备。 */
    mdns_txt_item_t txt[] = {
        {.key = "device", .value = identity->default_name},
        {.key = "fw", .value = identity->firmware_name},
    };
    if (mdns_service_add(NULL, "_http", "_tcp", CONFIG_KENKO_HTTP_PORT, txt, sizeof(txt) / sizeof(txt[0])) !=
        ESP_OK) {
        ESP_LOGW(TAG, "mdns service add failed");
    }

    s_ctx.mdns_running = true;
    ESP_LOGI(TAG, "mDNS started as %s.local", identity->mdns_hostname);
}

static void set_state(kenko_state_t state)
{
    kenko_state_t previous = kenko_state_get();
    if (previous == state) {
        return;
    }
    ESP_LOGI(TAG, "state %s -> %s", kenko_state_name(previous), kenko_state_name(state));
    kenko_state_set(state);
}

/** 配置分区挂不上时用红色闪烁把故障暴露出来，其余状态各有配色。 */
static void refresh_led(void)
{
    if (s_ctx.storage_degraded) {
        status_led_set(KENKO_LED_RED, LED_PATTERN_BLINK);
        return;
    }

    switch (kenko_state_get()) {
    case KENKO_STATE_PROVISIONING:
        status_led_set(KENKO_LED_BLUE, LED_PATTERN_SOLID);
        break;
    case KENKO_STATE_CONNECTING:
    case KENKO_STATE_OFFLINE:
        status_led_set(KENKO_LED_BLUE, LED_PATTERN_BREATHING);
        break;
    case KENKO_STATE_ONLINE:
        status_led_set(KENKO_LED_GREEN, LED_PATTERN_SOLID);
        break;
    case KENKO_STATE_BOOT:
    default:
        status_led_set(KENKO_LED_ORANGE, LED_PATTERN_SOLID);
        break;
    }
}

static void enter_provisioning(void)
{
    set_state(KENKO_STATE_PROVISIONING);
    stop_mdns();
    time_sync_stop();

    esp_err_t err = wifi_manager_start_provisioning_ap();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start provisioning AP failed: %s", esp_err_to_name(err));
    }
    dns_captive_start();
    refresh_led();

    /* 配网模式下设备同样在正常对外服务，足以判定这个镜像是好的。 */
    ota_service_mark_valid();
}

static void enter_connecting(void)
{
    set_state(KENKO_STATE_CONNECTING);
    dns_captive_stop();
    wifi_manager_stop_provisioning_ap();
    refresh_led();

    esp_err_t err = wifi_manager_start_sta_loop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start sta loop failed: %s", esp_err_to_name(err));
    }
}

static void enter_online(void)
{
    set_state(KENKO_STATE_ONLINE);
    dns_captive_stop();
    wifi_manager_stop_provisioning_ap();
    start_mdns();
    time_sync_start();
    refresh_led();

    /* 设备已经跑起来并能对外服务，可以确认这个镜像是好的。 */
    ota_service_mark_valid();
}

static void handle_wifi_lost(void)
{
    if (kenko_state_is_provisioning()) {
        return;
    }

    set_state(KENKO_STATE_OFFLINE);
    stop_mdns();
    time_sync_stop();
    refresh_led();

    esp_err_t err = wifi_manager_start_sta_loop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "restart sta loop failed: %s", esp_err_to_name(err));
    }
}

static void reboot_after_grace(void)
{
    ESP_LOGW(TAG, "rebooting in %d ms", CONFIG_KENKO_REBOOT_GRACE_MS);
    /* 留一点时间让 HTTP 响应发完，否则前端只会看到连接被重置。 */
    vTaskDelay(pdMS_TO_TICKS(CONFIG_KENKO_REBOOT_GRACE_MS));
    esp_restart();
}

static void handle_factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset requested");
    status_led_set(KENKO_LED_PURPLE, LED_PATTERN_BLINK);

    /* 先把内存里的状态清干净，再尽力格式化分区：即使格式化失败，
     * 重启后也不会再用旧凭据自动联网。 */
    wifi_config_store_clear();
    settings_store_reset();
    kenko_auth_reset();
    if (storage_fs_storage_available()) {
        storage_fs_format_storage();
    }

    reboot_after_grace();
}

static void handle_settings_changed(void)
{
    app_settings_t settings;
    settings_store_get(&settings);

    status_led_set_brightness(settings.led_brightness);
    time_sync_apply_timezone(settings.timezone);

    if (s_ctx.mdns_running) {
        mdns_instance_name_set(settings.device_name);
    }
    refresh_led();
}

static void app_state_task(void *arg)
{
    (void)arg;
    kenko_event_id_t event;

    for (;;) {
        if (xQueueReceive(s_ctx.queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event) {
        case KENKO_EVENT_START:
            if (!wifi_config_store_has_entries()) {
                ESP_LOGI(TAG, "no saved wifi config, entering provisioning");
                enter_provisioning();
            } else {
                enter_connecting();
            }
            break;

        case KENKO_EVENT_ENTER_PROVISIONING:
            if (!kenko_state_is_provisioning()) {
                enter_provisioning();
            }
            break;

        case KENKO_EVENT_APPLY_WIFI_CONFIG:
            if (wifi_config_store_has_entries()) {
                enter_connecting();
            } else {
                enter_provisioning();
            }
            break;

        case KENKO_EVENT_WIFI_CONNECTED:
            enter_online();
            break;

        case KENKO_EVENT_WIFI_LOST:
            handle_wifi_lost();
            break;

        case KENKO_EVENT_SETTINGS_CHANGED:
            handle_settings_changed();
            break;

        case KENKO_EVENT_REBOOT:
            reboot_after_grace();
            break;

        case KENKO_EVENT_FACTORY_RESET:
            handle_factory_reset();
            break;

        default:
            ESP_LOGW(TAG, "unhandled event %d", (int)event);
            break;
        }
    }
}

/** 事件回调只负责转投；真正的处理放在状态机任务里，不占用事件循环。 */
static void on_kenko_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;

    kenko_event_id_t event = (kenko_event_id_t)event_id;
    if (xQueueSend(s_ctx.queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped event %d", (int)event_id);
    }
}

esp_err_t app_state_start(void)
{
    kenko_state_set(KENKO_STATE_BOOT);
    s_ctx.storage_degraded = !storage_fs_storage_available();

    s_ctx.queue = xQueueCreate(APP_EVENT_QUEUE_LENGTH, sizeof(kenko_event_id_t));
    ESP_RETURN_ON_FALSE(s_ctx.queue != NULL, ESP_ERR_NO_MEM, TAG, "event queue alloc failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(KENKO_EVENT, ESP_EVENT_ANY_ID, &on_kenko_event, NULL, NULL), TAG,
        "register kenko event handler failed");

    /* Web 服务在两种模式下都要在线，一次启动后就不再停。
     * 起不来也不应该拖垮整个设备：联网、状态灯、按键这些仍然要工作。 */
    esp_err_t web_err = web_server_start();
    if (web_err != ESP_OK) {
        ESP_LOGE(TAG, "web server start failed: %s", esp_err_to_name(web_err));
    }

    BaseType_t created = xTaskCreate(app_state_task, "app_state", CONFIG_KENKO_STATE_TASK_STACK, NULL,
                                     CONFIG_KENKO_STATE_TASK_PRIORITY, NULL);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "state task create failed");

    return kenko_event_post(KENKO_EVENT_START);
}
