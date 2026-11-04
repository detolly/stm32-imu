#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "uart.h"

extern UART_HandleTypeDef huart3;

#define UART_LOG_HANDLE     (&huart3)

static SemaphoreHandle_t uart_mutex;

void uart_init(void)
{
    if (uart_mutex == NULL) {
        uart_mutex = xSemaphoreCreateMutex();
    }
}

static void uart_lock(void)
{
    if (uart_mutex != NULL) {
        (void)xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
}

static void uart_unlock(void)
{
    if (uart_mutex != NULL) {
        (void)xSemaphoreGive(uart_mutex);
    }
}

static void uart_transmit(const uint8_t *data, uint16_t length)
{
    (void)HAL_UART_Transmit(UART_LOG_HANDLE,
                            (uint8_t *)data,
                            length,
                            UART_LOG_TIMEOUT_MS);
}

static const char *uart_level_tag(uart_log_level_t level)
{
    switch (level) {
    case UART_LOG_LEVEL_ERROR:
        return "E";
    case UART_LOG_LEVEL_WARN:
        return "W";
    case UART_LOG_LEVEL_INFO:
        return "I";
    case UART_LOG_LEVEL_DEBUG:
        return "D";
    default:
        return "?";
    }
}

void uart_write_len(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U)) {
        return;
    }

    uart_lock();
    uart_transmit(data, length);
    uart_unlock();
}

static void uart_vlog(uart_log_level_t level, const char *fmt, va_list args)
{
    char buffer[UART_LOG_BUFFER_SIZE];
    int header_length;
    int body_length;
    size_t length;

    if ((fmt == NULL) || ((unsigned)level > (unsigned)UART_LOG_MIN_LEVEL)) {
        return;
    }

    /* Hold the mutex across formatting too: newlib's printf state is shared. */
    uart_lock();

    header_length = snprintf(buffer,
                             sizeof(buffer),
                             "[%8lu] [%s] ",
                             (unsigned long)HAL_GetTick(),
                             uart_level_tag(level));
    if (header_length < 0) {
        uart_unlock();
        return;
    }
    if ((size_t)header_length >= sizeof(buffer)) {
        header_length = (int)(sizeof(buffer) - 1U);
    }

    body_length = vsnprintf(buffer + header_length,
                            sizeof(buffer) - (size_t)header_length,
                            fmt,
                            args);
    if (body_length < 0) {
        uart_unlock();
        return;
    }

    length = (size_t)header_length + (size_t)body_length;
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1U;
    }

    if ((length + 2U) < sizeof(buffer)) {
        buffer[length++] = '\r';
        buffer[length++] = '\n';
        buffer[length] = '\0';
    }

    uart_transmit((const uint8_t *)buffer, (uint16_t)length);
    uart_unlock();
}

void uart_log(uart_log_level_t level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    uart_vlog(level, fmt, args);
    va_end(args);
}

void uart_log_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    uart_vlog(UART_LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

void uart_log_warn(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    uart_vlog(UART_LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

void uart_log_info(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    uart_vlog(UART_LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void uart_log_debug(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    uart_vlog(UART_LOG_LEVEL_DEBUG, fmt, args);
    va_end(args);
}
