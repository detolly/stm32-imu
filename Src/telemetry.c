#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "telemetry.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "uart.h"

#define TELEMETRY_QUEUE_LENGTH      8U
#define TELEMETRY_TASK_STACK_WORDS  512U
#define TELEMETRY_TASK_PRIORITY     (tskIDLE_PRIORITY + 3U)
#define TELEMETRY_RETRY_MS          1000U

static QueueHandle_t telemetry_queue;

static int send_all(int socket_fd, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t sent = 0U;

    while (sent < length) {
        int result = send(socket_fd, bytes + sent, length - sent, 0);

        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }

    return 0;
}

static void telemetry_task(void *argument)
{
    struct sockaddr_in server;
    telemetry_angles_t message;
    int enabled = 1;

    (void)argument;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(TELEMETRY_SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(TELEMETRY_SERVER_IP);

    uart_log_info("telemetry: started, server %s:%u",
                  TELEMETRY_SERVER_IP, (unsigned)TELEMETRY_SERVER_PORT);

    for (;;) {
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_fd < 0) {
            uart_log_error("telemetry: socket failed (errno=%d)", errno);
            vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
            continue;
        }

        (void)setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE,
                         &enabled, sizeof(enabled));
        (void)setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
                         &enabled, sizeof(enabled));

        if (connect(socket_fd, (struct sockaddr *)&server,
                    sizeof(server)) < 0) {
            uart_log_error("telemetry: connect failed (errno=%d)", errno);
            closesocket(socket_fd);
            vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
            continue;
        }

        uart_log_info("telemetry: connected");

        /* Drop readings queued before the connection was available. */
        (void)xQueueReset(telemetry_queue);

        for (;;) {
            if (xQueueReceive(telemetry_queue, &message,
                              portMAX_DELAY) != pdPASS) {
                break;
            }
            if (send_all(socket_fd, &message, sizeof(message)) != 0) {
                uart_log_error("telemetry: send failed (errno=%d)", errno);
                break;
            }
        }

        closesocket(socket_fd);
        uart_log_warn("telemetry: connection closed");
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
    }
}

int telemetry_init(void)
{
    telemetry_queue = xQueueCreate(TELEMETRY_QUEUE_LENGTH,
                                   sizeof(telemetry_angles_t));
    return (telemetry_queue != NULL) ? 0 : -1;
}

int telemetry_start(void)
{
    if (telemetry_queue == NULL) {
        return -1;
    }

    if (xTaskCreate(telemetry_task, "telemetry",
                    TELEMETRY_TASK_STACK_WORDS, NULL,
                    TELEMETRY_TASK_PRIORITY, NULL) != pdPASS) {
        return -1;
    }

    uart_log_info("telemetry: task created");
    return 0;
}

int telemetry_publish(float roll, float pitch)
{
    telemetry_angles_t message;

    if (telemetry_queue == NULL) {
        return 0;
    }

    message.roll = roll;
    message.pitch = pitch;

    return (xQueueSend(telemetry_queue, &message, 0U) == pdPASS) ? 1 : 0;
}
