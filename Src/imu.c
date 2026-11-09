#include <math.h>

#include "imu.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "led.h"
#include "mma8451.h"
#include "telemetry.h"
#include "uart.h"

/* Sample period matches the accelerometer ODR. */
#define SENSOR_PERIOD_MS        10U

#define RAD_TO_DEG              57.2957795f

/* xTaskCreate() stack depth is in words, not bytes. */
#define SENSOR_TASK_STACK_WORDS 384U
#define SENSOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 5U)

typedef struct {
    float roll;
    float pitch;
} angles_t;

static uint8_t read_sample(mma8451_accel_t *accel)
{
    uint8_t ready = 0U;

    if (mma8451_data_ready(&ready) != HAL_OK)
        return 0U;

    if (ready == 0U)
        return 0U;

    return (mma8451_read_accel(accel) == HAL_OK) ? 1U : 0U;
}

static angles_t calculate_angles(const mma8451_accel_t *accel)
{
    angles_t angles;

    /* Roll is rotation about X, pitch about Y. */
    angles.roll = atan2f(accel->y, accel->z) * RAD_TO_DEG;
    angles.pitch = atan2f(-accel->x,
                          sqrtf((accel->y * accel->y) +
                                (accel->z * accel->z))) *
                   RAD_TO_DEG;

    return angles;
}

static void sensor_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SENSOR_PERIOD_MS);

    (void)argument;

    for (;;) {
        mma8451_accel_t accel;

        if (read_sample(&accel) != 0U) {
            angles_t angles = calculate_angles(&accel);

            /* The LED strip follows pitch; both angles are reported over telemetry. */
            led_show_angle(angles.pitch);
            (void)telemetry_publish(angles.roll, angles.pitch);
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

void init(void)
{
    if (uart_init() != 0) {
        uart_log_error("UART initialization failed");
        Error_Handler();
    }

    if (mma8451_init() != HAL_OK) {
        uart_log_error("accelerometer initialization failed");
        Error_Handler();
    }

    if (telemetry_init() != 0) {
        uart_log_error("telemetry queue creation failed");
        Error_Handler();
    }

    if (xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK_WORDS, NULL,
                    SENSOR_TASK_PRIORITY, NULL) != pdPASS) {
        uart_log_error("sensor task creation failed");
        Error_Handler();
    }
}
