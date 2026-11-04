#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32f7xx_hal.h"

#define UART_LOG_BUFFER_SIZE    128U
#define UART_LOG_TIMEOUT_MS     100U

typedef enum {
    UART_LOG_LEVEL_ERROR = 0,
    UART_LOG_LEVEL_WARN  = 1,
    UART_LOG_LEVEL_INFO  = 2,
    UART_LOG_LEVEL_DEBUG = 3
} uart_log_level_t;

/* Messages with a level above this value are discarded at runtime. */
#ifndef UART_LOG_MIN_LEVEL
#define UART_LOG_MIN_LEVEL      UART_LOG_LEVEL_INFO
#endif

#if defined(__GNUC__)
#define UART_PRINTF_LIKE(fmt_index, arg_index) \
    __attribute__((format(printf, fmt_index, arg_index)))
#else
#define UART_PRINTF_LIKE(fmt_index, arg_index)
#endif

/* Creates the mutex that serializes UART access between tasks. */
void uart_init(void);

void uart_write_len(const uint8_t *data, uint16_t length);

void uart_log(uart_log_level_t level, const char *fmt, ...)
    UART_PRINTF_LIKE(2, 3);

void uart_log_error(const char *fmt, ...) UART_PRINTF_LIKE(1, 2);
void uart_log_warn(const char *fmt, ...)  UART_PRINTF_LIKE(1, 2);
void uart_log_info(const char *fmt, ...)  UART_PRINTF_LIKE(1, 2);
void uart_log_debug(const char *fmt, ...) UART_PRINTF_LIKE(1, 2);

#endif /* UART_H */
