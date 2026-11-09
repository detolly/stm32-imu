#ifndef MMA8451_H
#define MMA8451_H

#include "stm32f7xx_hal.h"

/* SA0 is tied low on this board. */
#define MMA8451_I2C_ADDR            (0x1CU << 1)
#define MMA8451_I2C_TIMEOUT_MS      100U

#define MMA8451_REG_STATUS          0x00U
#define MMA8451_REG_OUT_X_MSB       0x01U
#define MMA8451_REG_WHO_AM_I        0x0DU
#define MMA8451_REG_XYZ_DATA_CFG    0x0EU
#define MMA8451_REG_CTRL_REG1       0x2AU
#define MMA8451_REG_CTRL_REG2       0x2BU

#define MMA8451_WHO_AM_I_VALUE      0x1AU
#define MMA8451_STATUS_ZYXDR        (1U << 3)
#define MMA8451_CTRL_REG1_ACTIVE    (1U << 0)
#define MMA8451_CTRL_REG1_DR_100HZ  (3U << 3)
#define MMA8451_CTRL_REG2_MODS_HIGH_RES (2U << 0)

typedef struct {
    float x;                    /* g */
    float y;                    /* g */
    float z;                    /* g */
} mma8451_accel_t;

/* Configures the sensor for +/-2 g, 100 Hz, high resolution. */
HAL_StatusTypeDef mma8451_init(void);

HAL_StatusTypeDef mma8451_data_ready(uint8_t *ready);
HAL_StatusTypeDef mma8451_read_accel(mma8451_accel_t *accel);

#endif /* MMA8451_H */
