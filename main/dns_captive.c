#include "dns_captive.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "app_config.h"
#include "dns_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "dns_captive";

/* recvfrom 的超时，决定了停止服务时最坏要等多久。 */
#define DNS_RECV_TIMEOUT_MS 300
#define DNS_STOP_TIMEOUT_MS 3000
#define DNS_REQUEST_MAX_LEN 512

static volatile bool s_running;
static int s_socket = -1;
static SemaphoreHandle_t s_exited;

static void dns_task(void *arg)
{
    (void)arg;

    uint8_t request[DNS_REQUEST_MAX_LEN];
    uint8_t response[DNS_REQUEST_MAX_LEN + DNS_MESSAGE_ANSWER_LEN];
    struct in_addr answer_addr = {0};
    inet_aton(KENKO_AP_IP_ADDR, &answer_addr);

    while (s_running) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_len = sizeof(client_addr);
        ssize_t received =
            recvfrom(s_socket, request, sizeof(request), 0, (struct sockaddr *)&client_addr, &client_len);

        if (received <= 0) {
            /* 超时是正常路径：它给了这个循环重新检查停止标志的机会。 */
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            if (s_running) {
                ESP_LOGW(TAG, "recvfrom failed: %d", errno);
            }
            break;
        }

        size_t response_len = dns_message_build_response(request, (size_t)received, answer_addr.s_addr,
                                                         KENKO_DNS_TTL_SECONDS, response, sizeof(response));
        if (response_len > 0) {
            sendto(s_socket, response, response_len, 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    /* socket 只在持有它的任务里关闭，避免另一个任务把它从 recvfrom 下面抽走。 */
    if (s_socket >= 0) {
        close(s_socket);
        s_socket = -1;
    }
    s_running = false;

    ESP_LOGI(TAG, "dns captive server stopped");
    xSemaphoreGive(s_exited);
    vTaskDelete(NULL);
}

esp_err_t dns_captive_start(void)
{
    if (s_running) {
        return ESP_OK;
    }

    if (s_exited == NULL) {
        s_exited = xSemaphoreCreateBinary();
        if (s_exited == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_socket < 0) {
        ESP_LOGE(TAG, "socket failed: %d", errno);
        return ESP_FAIL;
    }

    const struct timeval timeout = {
        .tv_sec = DNS_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (DNS_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    if (setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        ESP_LOGE(TAG, "set recv timeout failed: %d", errno);
        close(s_socket);
        s_socket = -1;
        return ESP_FAIL;
    }

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(KENKO_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_socket, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind failed: %d", errno);
        close(s_socket);
        s_socket = -1;
        return ESP_FAIL;
    }

    s_running = true;
    BaseType_t created =
        xTaskCreate(dns_task, "dns_captive", KENKO_TASK_STACK_DNS, NULL, KENKO_TASK_PRIORITY_DNS, NULL);
    if (created != pdPASS) {
        s_running = false;
        close(s_socket);
        s_socket = -1;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "dns captive server started on %s:%d", KENKO_AP_IP_ADDR, KENKO_DNS_PORT);
    return ESP_OK;
}

void dns_captive_stop(void)
{
    if (!s_running) {
        return;
    }

    s_running = false;
    if (xSemaphoreTake(s_exited, pdMS_TO_TICKS(DNS_STOP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "dns task did not exit within %d ms", DNS_STOP_TIMEOUT_MS);
    }
}

bool dns_captive_is_running(void)
{
    return s_running;
}
