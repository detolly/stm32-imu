#include <math.h>

#include "start.h"

#include "main.h"
#include "led.h"
#include "mma8451.h"
#include "uart.h"

/* Poll slightly faster than the sensor ODR so a fresh sample is always
 * available when the loop runs. */
#define POLL_PERIOD_MS    10U

#define RAD_TO_DEG        57.2957795f

static uint32_t next_poll_tick;

typedef struct {
    float roll;
    float pitch;
} angles_t;

static int32_t accel_to_mg(float g)
{
    return (int32_t)((g * 1000.0f) + ((g >= 0.0f) ? 0.5f : -0.5f));
}

static uint8_t sample_due(uint32_t now)
{
    if ((int32_t)(now - next_poll_tick) < 0) {
        return 0U;
    }

    next_poll_tick += POLL_PERIOD_MS;
    return 1U;
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

static void print_sample(const mma8451_accel_t *accel,
                         const angles_t *angles)
{
    uart_log_info("x=%ld mg y=%ld mg z=%ld mg roll=%.1f deg pitch=%.1f deg",
                  (long)accel_to_mg(accel->x),
                  (long)accel_to_mg(accel->y),
                  (long)accel_to_mg(accel->z),
                  (double)angles->roll,
                  (double)angles->pitch);
}

void init(void)
{
    if (mma8451_init(MMA8451_ADDR_SA0_LOW) != HAL_OK) {
        uart_log_error("accelerometer initialization failed");
        Error_Handler();
    }

    if (mma8451_set_data_rate(MMA8451_ODR_100HZ) != HAL_OK) {
        uart_log_error("setting data rate failed");
        Error_Handler();
    }

    next_poll_tick = HAL_GetTick();
}

void loop(void)
{
    mma8451_accel_t accel;
    angles_t angles;

    if (sample_due(HAL_GetTick()) == 0U) {
        return;
    }
    if (read_sample(&accel) == 0U) {
        return;
    }

    angles = calculate_angles(&accel);

    print_sample(&accel, &angles);
    led_show_angle(angles.pitch);
}
