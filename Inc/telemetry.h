#ifndef TELEMETRY_H
#define TELEMETRY_H

/* Telemetry server endpoint. */
#ifndef TELEMETRY_SERVER_IP
#define TELEMETRY_SERVER_IP "10.0.1.1"
#endif

#ifndef TELEMETRY_SERVER_PORT
#define TELEMETRY_SERVER_PORT 5000U
#endif

/* Binary payload sent for each sample: roll then pitch, in degrees. */
typedef struct {
    float roll;
    float pitch;
} telemetry_angles_t;

/* Creates the outbound message queue. Call before the scheduler starts. */
int telemetry_init(void);

/* Creates the TCP client task. */
int telemetry_start(void);

/* Queues one angle pair without blocking; returns 1 on success. */
int telemetry_publish(float roll, float pitch);

#endif /* TELEMETRY_H */
