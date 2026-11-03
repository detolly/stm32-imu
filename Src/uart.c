#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "uart.h"

extern UART_HandleTypeDef huart3;

#define UART_LOG_HANDLE     (&huart3)

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

    (void)HAL_UART_Transmit(UART_LOG_HANDLE,
                            (uint8_t *)data,
                            length,
                            UART_LOG_TIMEOUT_MS);
}

void uart_write(const char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    if (length > UINT16_MAX) {
        length = UINT16_MAX;
    }

    uart_write_len((const uint8_t *)text, (uint16_t)length);
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

    header_length = snprintf(buffer,
                             sizeof(buffer),
                             "[%8lu] [%s] ",
                             (unsigned long)HAL_GetTick(),
                             uart_level_tag(level));
    if (header_length < 0) {
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

    uart_write_len((const uint8_t *)buffer, (uint16_t)length);
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
