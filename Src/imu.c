#include <math.h>

#include "imu.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "main.h"
#include "led.h"
#include "mma8451.h"
#include "telemetry.h"
#include "uart.h"

/* Sample period matches the accelerometer ODR. */
#define SENSOR_PERIOD_MS        10U

#define RAD_TO_DEG              57.2957795f

/* Keep a few samples of slack for the logger; the sensor drops when full. */
#define SAMPLE_QUEUE_LENGTH     8U

/* xTaskCreate() stack depth is in words, not bytes. */
#define SENSOR_TASK_STACK_WORDS 384U
#define LOGGER_TASK_STACK_WORDS 512U

#define SENSOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 5U)
#define LOGGER_TASK_PRIORITY    (tskIDLE_PRIORITY + 2U)

typedef struct {
    float roll;
    float pitch;
} angles_t;

typedef struct {
    mma8451_accel_t accel;
    angles_t angles;
} sample_t;

static QueueHandle_t sample_queue;

static int32_t accel_to_mg(float g)
{
    return (int32_t)((g * 1000.0f) + ((g >= 0.0f) ? 0.5f : -0.5f));
}

static uint8_t read_sample(mma8451_accel_t *accel)
{
    uint8_t ready = 0U;

    if (mma8451_data_ready(&ready) != HAL_OK) {
        return 0U;
    }
    if (ready == 0U) {
        return 0U;
    }

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

static void print_sample(const sample_t *sample)
{
    uart_log_info("x=%ld mg y=%ld mg z=%ld mg roll=%.1f deg pitch=%.1f deg",
                  (long)accel_to_mg(sample->accel.x),
                  (long)accel_to_mg(sample->accel.y),
                  (long)accel_to_mg(sample->accel.z),
                  (double)sample->angles.roll,
                  (double)sample->angles.pitch);
}

static void sensor_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SENSOR_PERIOD_MS);

    (void)argument;

    for (;;) {
        mma8451_accel_t accel;

        if (read_sample(&accel) != 0U) {
            sample_t sample;

            sample.accel = accel;
            sample.angles = calculate_angles(&accel);

            /* The LED strip follows pitch; roll is only logged. */
            led_show_angle(sample.angles.pitch);

            /* Never block the sampling task on the logger. */
            (void)xQueueSend(sample_queue, &sample, 0U);
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

static void logger_task(void *argument)
{
    sample_t sample;

    (void)argument;

    for (;;) {
        if (xQueueReceive(sample_queue, &sample, portMAX_DELAY) == pdPASS) {
            print_sample(&sample);
            (void)telemetry_publish(sample.angles.roll, sample.angles.pitch);
        }
    }
}

void init(void)
{
    uart_init();

    if (mma8451_init(MMA8451_ADDR_SA0_LOW) != HAL_OK) {
        uart_log_error("accelerometer initialization failed");
        Error_Handler();
    }

    if (mma8451_set_data_rate(MMA8451_ODR_100HZ) != HAL_OK) {
        uart_log_error("setting data rate failed");
        Error_Handler();
    }

    sample_queue = xQueueCreate(SAMPLE_QUEUE_LENGTH, sizeof(sample_t));
    if (sample_queue == NULL) {
        uart_log_error("sample queue creation failed");
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

    if (xTaskCreate(logger_task, "logger", LOGGER_TASK_STACK_WORDS, NULL,
                    LOGGER_TASK_PRIORITY, NULL) != pdPASS) {
        uart_log_error("logger task creation failed");
        Error_Handler();
    }
}
