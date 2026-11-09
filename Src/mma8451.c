#include <stddef.h>

#include "mma8451.h"

/* I2C handle created by MX_I2C2_Init() in main.c. */
extern I2C_HandleTypeDef hi2c2;


static HAL_StatusTypeDef read_regs(uint8_t reg, uint8_t *data,
                                   uint16_t length)
{
    return HAL_I2C_Mem_Read(&hi2c2, MMA8451_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, data, length,
                            MMA8451_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c2, MMA8451_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &value, 1U,
                             MMA8451_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef mma8451_init(void)
{
    HAL_StatusTypeDef status;
    uint8_t who_am_i = 0U;

    status = read_regs(MMA8451_REG_WHO_AM_I, &who_am_i, 1U);
    if (status != HAL_OK) {
        return status;
    }
    if (who_am_i != MMA8451_WHO_AM_I_VALUE) {
        return HAL_ERROR;
    }

    /* Configuration registers can only be changed in standby. */
    status = write_reg(MMA8451_REG_CTRL_REG1, 0U);
    if (status != HAL_OK) {
        return status;
    }

    /* XYZ_DATA_CFG = 0 selects +/-2 g. */
    status = write_reg(MMA8451_REG_XYZ_DATA_CFG, 0U);
    if (status != HAL_OK) {
        return status;
    }

    status = write_reg(MMA8451_REG_CTRL_REG2,
                       MMA8451_CTRL_REG2_MODS_HIGH_RES);
    if (status != HAL_OK) {
        return status;
    }

    return write_reg(MMA8451_REG_CTRL_REG1,
                     MMA8451_CTRL_REG1_DR_100HZ |
                     MMA8451_CTRL_REG1_ACTIVE);
}

HAL_StatusTypeDef mma8451_data_ready(uint8_t *ready)
{
    HAL_StatusTypeDef status;
    uint8_t status_reg = 0U;

    if (ready == NULL) {
        return HAL_ERROR;
    }

    status = read_regs(MMA8451_REG_STATUS, &status_reg, 1U);
    if (status != HAL_OK) {
        return status;
    }

    *ready = ((status_reg & MMA8451_STATUS_ZYXDR) != 0U) ? 1U : 0U;

    return HAL_OK;
}

static int16_t parse_axis(uint8_t msb, uint8_t lsb)
{
    /* The 14-bit sample is left aligned in the 16-bit output registers:
     * MSB[7:0] = D13..D6, LSB[7:2] = D5..D0. */
    uint16_t value = (uint16_t)(((uint16_t)msb << 8) | lsb) >> 2;

    if ((value & 0x2000U) != 0U) {
        value |= 0xC000U;
    }

    return (int16_t)value;
}

HAL_StatusTypeDef mma8451_read_accel(mma8451_accel_t *accel)
{
    HAL_StatusTypeDef status;
    uint8_t buffer[6];

    if (accel == NULL) {
        return HAL_ERROR;
    }

    /* Reading OUT_X_MSB..OUT_Z_LSB in one transaction keeps the three axes
     * from the same sample and clears the data-ready flags. */
    status = read_regs(MMA8451_REG_OUT_X_MSB, buffer, sizeof(buffer));
    if (status != HAL_OK) {
        return status;
    }

    /* +/-2 g: 4096 counts per g. */
    accel->x = (float)parse_axis(buffer[0], buffer[1]) / 4096.0f;
    accel->y = (float)parse_axis(buffer[2], buffer[3]) / 4096.0f;
    accel->z = (float)parse_axis(buffer[4], buffer[5]) / 4096.0f;

    return HAL_OK;
}
