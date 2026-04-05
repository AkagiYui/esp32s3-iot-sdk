#include "dns_captive.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "app_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dns_captive";

static TaskHandle_t s_dns_task;
static int s_dns_socket = -1;

static size_t build_response(const uint8_t *request, size_t request_len, uint8_t *response, size_t response_size)
{
    if (request_len < 12 || response_size < request_len + 16) {
        return 0;
    }

    memcpy(response, request, request_len);
    response[2] = 0x81;
    response[3] = 0x80;
    response[6] = 0x00;
    response[7] = 0x01;

    size_t cursor = request_len;
    response[cursor++] = 0xc0;
    response[cursor++] = 0x0c;
    response[cursor++] = 0x00;
    response[cursor++] = 0x01;
    response[cursor++] = 0x00;
    response[cursor++] = 0x01;
    response[cursor++] = 0x00;
    response[cursor++] = 0x00;
    response[cursor++] = 0x00;
    response[cursor++] = 0x3c;
    response[cursor++] = 0x00;
    response[cursor++] = 0x04;

    struct in_addr addr;
    inet_aton(KENKO_AP_IP_ADDR, &addr);
    memcpy(&response[cursor], &addr.s_addr, 4);
    cursor += 4;
    return cursor;
}

static void dns_task(void *arg)
{
    (void)arg;
    uint8_t request[512];
    uint8_t response[528];

    while (s_dns_socket >= 0) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_len = sizeof(client_addr);
        ssize_t received = recvfrom(s_dns_socket, request, sizeof(request), 0, (struct sockaddr *)&client_addr, &client_len);
        if (received <= 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        size_t response_len = build_response(request, (size_t)received, response, sizeof(response));
        if (response_len > 0) {
            sendto(s_dns_socket, response, response_len, 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    s_dns_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t dns_captive_start(void)
{
    if (s_dns_task != NULL) {
        return ESP_OK;
    }

    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_socket < 0) {
        return ESP_FAIL;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(KENKO_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_dns_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
        return ESP_FAIL;
    }

    xTaskCreate(dns_task, "dns_captive", 4096, NULL, 4, &s_dns_task);
    ESP_LOGI(TAG, "dns captive server started on %s:%d", KENKO_AP_IP_ADDR, KENKO_DNS_PORT);
    return ESP_OK;
}

void dns_captive_stop(void)
{
    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    s_dns_task = NULL;
}