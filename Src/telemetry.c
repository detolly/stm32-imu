#include <stdint.h>
#include <string.h>

#include "telemetry.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#define TELEMETRY_QUEUE_LENGTH      8U
#define TELEMETRY_TASK_STACK_WORDS  512U
#define TELEMETRY_TASK_PRIORITY     (tskIDLE_PRIORITY + 3U)
#define TELEMETRY_RETRY_MS          1000U

#define TELEMETRY_KEEP_IDLE_S       5
#define TELEMETRY_KEEP_INTERVAL_S   2
#define TELEMETRY_KEEP_COUNT        3
#define TELEMETRY_SEND_TIMEOUT_S    1

typedef struct {
    char text[TELEMETRY_MESSAGE_MAX];
} telemetry_message_t;

static QueueHandle_t telemetry_queue;

static int send_all(int socket, const char *data, size_t length)
{
    size_t sent = 0U;

    while (sent < length) {
        int result = send(socket, data + sent, length - sent, 0);

        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }

    return 0;
}

static void configure_socket(int socket)
{
    int enabled = 1;

    (void)setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &enabled,
                     sizeof(enabled));
    (void)setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled,
                     sizeof(enabled));

#if LWIP_TCP_KEEPALIVE
    {
        int idle = TELEMETRY_KEEP_IDLE_S;
        int interval = TELEMETRY_KEEP_INTERVAL_S;
        int count = TELEMETRY_KEEP_COUNT;

        (void)setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle,
                         sizeof(idle));
        (void)setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
                         sizeof(interval));
        (void)setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &count,
                         sizeof(count));
    }
#endif

#if LWIP_SO_SNDTIMEO
    {
        struct timeval timeout;

        timeout.tv_sec = TELEMETRY_SEND_TIMEOUT_S;
        timeout.tv_usec = 0;
        (void)setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                         sizeof(timeout));
    }
#endif
}

static void telemetry_task(void *argument)
{
    struct sockaddr_in server;
    telemetry_message_t message;

    (void)argument;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(TELEMETRY_SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(TELEMETRY_SERVER_IP);

    for (;;) {
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
            continue;
        }

        configure_socket(socket_fd);

        if (connect(socket_fd, (struct sockaddr *)&server,
                    sizeof(server)) < 0) {
            closesocket(socket_fd);
            vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
            continue;
        }

        /* Drop readings queued before the connection was available. */
        (void)xQueueReset(telemetry_queue);

        for (;;) {
            if (xQueueReceive(telemetry_queue, &message,
                              portMAX_DELAY) != pdPASS) {
                break;
            }
            if (send_all(socket_fd, message.text,
                         strlen(message.text)) != 0) {
                break;
            }
        }

        closesocket(socket_fd);
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
    }
}

int telemetry_init(void)
{
    telemetry_queue = xQueueCreate(TELEMETRY_QUEUE_LENGTH,
                                   sizeof(telemetry_message_t));
    return (telemetry_queue != NULL) ? 0 : -1;
}

int telemetry_start(void)
{
    if (telemetry_queue == NULL) {
        return -1;
    }

    return (xTaskCreate(telemetry_task, "telemetry",
                        TELEMETRY_TASK_STACK_WORDS, NULL,
                        TELEMETRY_TASK_PRIORITY, NULL) == pdPASS) ? 0 : -1;
}

int telemetry_publish(const char *message)
{
    telemetry_message_t item;

    if ((telemetry_queue == NULL) || (message == NULL)) {
        return 0;
    }

    strncpy(item.text, message, sizeof(item.text) - 1U);
    item.text[sizeof(item.text) - 1U] = '\0';

    return (xQueueSend(telemetry_queue, &item, 0U) == pdPASS) ? 1 : 0;
}
