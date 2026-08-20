#include "time_sync.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "app_config.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "settings_store.h"

static const char *TAG = "time_sync";

/* 2023-01-01 之前的时间一定是没同步过的默认值。 */
#define TIME_SYNC_EPOCH_THRESHOLD 1672531200

static bool s_sntp_running;

static void on_time_synced(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "system time synchronized");
}

void time_sync_apply_timezone(const char *timezone)
{
    if (timezone == NULL || timezone[0] == '\0') {
        timezone = KENKO_DEFAULT_TIMEZONE;
    }

    setenv("TZ", timezone, 1);
    tzset();
    ESP_LOGI(TAG, "timezone set to %s", timezone);
}

esp_err_t time_sync_init(void)
{
    app_settings_t settings;
    settings_store_get(&settings);
    time_sync_apply_timezone(settings.timezone);
    return ESP_OK;
}

esp_err_t time_sync_start(void)
{
    app_settings_t settings;
    settings_store_get(&settings);

    if (!settings.ntp_enabled) {
        ESP_LOGI(TAG, "ntp disabled by settings");
        return ESP_OK;
    }
    if (s_sntp_running) {
        return ESP_OK;
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        2, ESP_SNTP_SERVER_LIST(KENKO_SNTP_SERVER_PRIMARY, KENKO_SNTP_SERVER_SECONDARY));
    config.start = true;
    config.sync_cb = on_time_synced;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sntp init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_sntp_running = true;
    ESP_LOGI(TAG, "sntp started (%s, %s)", KENKO_SNTP_SERVER_PRIMARY, KENKO_SNTP_SERVER_SECONDARY);
    return ESP_OK;
}

void time_sync_stop(void)
{
    if (!s_sntp_running) {
        return;
    }

    esp_netif_sntp_deinit();
    s_sntp_running = false;
    ESP_LOGI(TAG, "sntp stopped");
}

bool time_sync_is_synced(void)
{
    time_t now = 0;
    time(&now);
    return now > TIME_SYNC_EPOCH_THRESHOLD;
}

void time_sync_snapshot(char *iso8601, size_t size, int64_t *epoch_seconds)
{
    time_t now = 0;
    time(&now);

    if (epoch_seconds != NULL) {
        *epoch_seconds = (int64_t)now;
    }

    if (iso8601 == NULL || size == 0) {
        return;
    }

    struct tm local = {0};
    localtime_r(&now, &local);
    if (strftime(iso8601, size, "%Y-%m-%dT%H:%M:%S%z", &local) == 0) {
        iso8601[0] = '\0';
    }
}
