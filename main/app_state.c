#include "app_state.h"

#include <string.h>

#include "app_config.h"
#include "device_info.h"
#include "dns_captive.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mdns.h"
#include "ota_service.h"
#include "settings_store.h"
#include "status_led.h"
#include "storage_fs.h"
#include "time_sync.h"
#include "web_server.h"
#include "wifi_config_store.h"
#include "wifi_manager.h"

static const char *TAG = "app_state";

#define APP_EVENT_QUEUE_LENGTH 8
#define APP_REBOOT_GRACE_MS 800

/* 状态机的上下文是模块内的静态实例：app_main 返回后它的栈会被回收，
 * 任何指向那块内存的指针都会悬空。 */
static struct {
    QueueHandle_t queue;
    app_state_t state;
    bool mdns_running;
    bool storage_degraded;
} s_ctx;

const char *app_state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:
        return "boot";
    case APP_STATE_PROVISIONING:
        return "provisioning";
    case APP_STATE_CONNECTING:
        return "connecting";
    case APP_STATE_ONLINE:
        return "online";
    case APP_STATE_OFFLINE:
        return "offline";
    default:
        return "unknown";
    }
}

app_state_t app_state_get(void)
{
    return s_ctx.state;
}

bool app_state_is_provisioning(void)
{
    return s_ctx.state == APP_STATE_PROVISIONING;
}

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
    if (mdns_service_add(NULL, "_http", "_tcp", KENKO_HTTP_PORT, txt, sizeof(txt) / sizeof(txt[0])) !=
        ESP_OK) {
        ESP_LOGW(TAG, "mdns service add failed");
    }

    s_ctx.mdns_running = true;
    ESP_LOGI(TAG, "mDNS started as %s.local", identity->mdns_hostname);
}

static void set_state(app_state_t state)
{
    if (s_ctx.state == state) {
        return;
    }
    ESP_LOGI(TAG, "state %s -> %s", app_state_name(s_ctx.state), app_state_name(state));
    s_ctx.state = state;
}

/** 配置分区挂不上时用红色闪烁把故障暴露出来，其余状态各有配色。 */
static void refresh_led(void)
{
    if (s_ctx.storage_degraded) {
        status_led_set(KENKO_LED_RED, LED_PATTERN_BLINK);
        return;
    }

    switch (s_ctx.state) {
    case APP_STATE_PROVISIONING:
        status_led_set(KENKO_LED_BLUE, LED_PATTERN_SOLID);
        break;
    case APP_STATE_CONNECTING:
    case APP_STATE_OFFLINE:
        status_led_set(KENKO_LED_BLUE, LED_PATTERN_BREATHING);
        break;
    case APP_STATE_ONLINE:
        status_led_set(KENKO_LED_GREEN, LED_PATTERN_SOLID);
        break;
    case APP_STATE_BOOT:
    default:
        status_led_set(KENKO_LED_ORANGE, LED_PATTERN_SOLID);
        break;
    }
}

static void enter_provisioning(void)
{
    set_state(APP_STATE_PROVISIONING);
    stop_mdns();
    time_sync_stop();
    wifi_manager_start_provisioning_ap();
    dns_captive_start();
    refresh_led();

    /* 配网模式下设备同样在正常对外服务，足以判定这个镜像是好的。 */
    ota_service_mark_valid();
}

static void enter_connecting(void)
{
    set_state(APP_STATE_CONNECTING);
    dns_captive_stop();
    wifi_manager_stop_provisioning_ap();
    refresh_led();
    wifi_manager_start_sta_loop();
}

static void enter_online(void)
{
    set_state(APP_STATE_ONLINE);
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
    if (s_ctx.state == APP_STATE_PROVISIONING) {
        return;
    }

    set_state(APP_STATE_OFFLINE);
    stop_mdns();
    time_sync_stop();
    refresh_led();
    wifi_manager_start_sta_loop();
}

static void reboot_after_grace(void)
{
    ESP_LOGW(TAG, "rebooting in %d ms", APP_REBOOT_GRACE_MS);
    /* 留一点时间让 HTTP 响应发完，否则前端只会看到连接被重置。 */
    vTaskDelay(pdMS_TO_TICKS(APP_REBOOT_GRACE_MS));
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
    app_event_t event;

    for (;;) {
        if (xQueueReceive(s_ctx.queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event) {
        case APP_EVENT_START:
            if (!wifi_config_store_has_entries()) {
                ESP_LOGI(TAG, "no saved wifi config, entering provisioning");
                enter_provisioning();
            } else {
                enter_connecting();
            }
            break;

        case APP_EVENT_ENTER_PROVISIONING:
            if (s_ctx.state != APP_STATE_PROVISIONING) {
                enter_provisioning();
            }
            break;

        case APP_EVENT_APPLY_WIFI_CONFIG:
            if (wifi_config_store_has_entries()) {
                enter_connecting();
            } else {
                enter_provisioning();
            }
            break;

        case APP_EVENT_WIFI_CONNECTED:
            enter_online();
            break;

        case APP_EVENT_WIFI_LOST:
            handle_wifi_lost();
            break;

        case APP_EVENT_SETTINGS_CHANGED:
            handle_settings_changed();
            break;

        case APP_EVENT_REBOOT:
            reboot_after_grace();
            break;

        case APP_EVENT_FACTORY_RESET:
            handle_factory_reset();
            break;

        default:
            ESP_LOGW(TAG, "unhandled event %d", (int)event);
            break;
        }
    }
}

esp_err_t app_state_start(void)
{
    s_ctx.state = APP_STATE_BOOT;
    s_ctx.storage_degraded = !storage_fs_storage_available();

    s_ctx.queue = xQueueCreate(APP_EVENT_QUEUE_LENGTH, sizeof(app_event_t));
    ESP_RETURN_ON_FALSE(s_ctx.queue != NULL, ESP_ERR_NO_MEM, TAG, "event queue alloc failed");

    /* Web 服务在两种模式下都要在线，一次启动后就不再停。 */
    ESP_RETURN_ON_ERROR(web_server_start(), TAG, "web server start failed");

    BaseType_t created = xTaskCreate(app_state_task, "app_state", KENKO_TASK_STACK_STATE, NULL,
                                     KENKO_TASK_PRIORITY_STATE, NULL);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "state task create failed");

    app_state_post_event(APP_EVENT_START);
    return ESP_OK;
}

void app_state_post_event(app_event_t event)
{
    if (s_ctx.queue == NULL) {
        return;
    }

    if (xQueueSend(s_ctx.queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped event %d", (int)event);
    }
}
