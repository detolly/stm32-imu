#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stddef.h>

/* Telemetry server endpoint. Change these to point at another host. */
#ifndef TELEMETRY_SERVER_IP
#define TELEMETRY_SERVER_IP "10.0.0.81"
#endif

#ifndef TELEMETRY_SERVER_PORT
#define TELEMETRY_SERVER_PORT 5000U
#endif

#define TELEMETRY_MESSAGE_MAX 96U

/* Creates the outbound message queue. Call before the scheduler starts. */
int telemetry_init(void);

/* Creates the TCP client task. Call after MX_LWIP_Init(). */
int telemetry_start(void);

/* Queues one newline-terminated message without blocking; returns 1 on success. */
int telemetry_publish(const char *message, size_t length);

#endif /* TELEMETRY_H */
