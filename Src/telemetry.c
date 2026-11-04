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

#define TELEMETRY_KEEP_IDLE_S       5
#define TELEMETRY_KEEP_INTERVAL_S   2
#define TELEMETRY_KEEP_COUNT        3
#define TELEMETRY_SEND_TIMEOUT_S    1
#define TELEMETRY_CONNECT_TIMEOUT_MS 2000

static QueueHandle_t telemetry_queue;

static int connect_with_timeout(int socket_fd,
                                const struct sockaddr_in *server)
{
    struct pollfd pfd;
    socklen_t error_length = sizeof(int);
    int flags;
    int error = 0;
    int result;

    flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    result = connect(socket_fd, (const struct sockaddr *)server,
                     sizeof(*server));
    if ((result < 0) && (errno != EINPROGRESS) && (errno != EALREADY)) {
        (void)fcntl(socket_fd, F_SETFL, flags);
        return -1;
    }

    if (result < 0) {
        pfd.fd = socket_fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;

        result = poll(&pfd, 1, TELEMETRY_CONNECT_TIMEOUT_MS);
        if (result <= 0) {
            if (result == 0) {
                errno = ETIMEDOUT;
            }
            (void)fcntl(socket_fd, F_SETFL, flags);
            return -1;
        }

        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                       &error, &error_length) < 0) {
            (void)fcntl(socket_fd, F_SETFL, flags);
            return -1;
        }
        if (error != 0) {
            errno = error;
            (void)fcntl(socket_fd, F_SETFL, flags);
            return -1;
        }
    }

    (void)fcntl(socket_fd, F_SETFL, flags);
    return 0;
}

static int send_all(int socket, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t sent = 0U;

    while (sent < length) {
        int result = send(socket, bytes + sent, length - sent, 0);

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
    telemetry_angles_t message;
    uint32_t attempt = 0U;

    (void)argument;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(TELEMETRY_SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(TELEMETRY_SERVER_IP);

    uart_log_info("telemetry: task started, server %s:%u",
                  TELEMETRY_SERVER_IP, (unsigned)TELEMETRY_SERVER_PORT);

    for (;;) {
        int socket_fd;

        uart_log_info("telemetry: connecting to %s:%u (attempt %lu)",
                      TELEMETRY_SERVER_IP, (unsigned)TELEMETRY_SERVER_PORT,
                      (unsigned long)++attempt);

        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            uart_log_error("telemetry: socket creation failed (errno=%d)",
                           errno);
            vTaskDelay(pdMS_TO_TICKS(TELEMETRY_RETRY_MS));
            continue;
        }

        configure_socket(socket_fd);

        if (connect_with_timeout(socket_fd, &server) < 0) {
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
