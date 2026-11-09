#include <stddef.h>

#include "mma8451.h"
#include "uart.h"

/* I2C handle created by MX_I2C2_Init() in main.c. */
extern I2C_HandleTypeDef hi2c2;

#define MMA8451_I2C_HANDLE      (&hi2c2)

#define MMA8451_FAIL(where, status)                                      \
    do {                                                                 \
        uart_log_error("MMA8451: %s failed (HAL status %d)",             \
                       (where), (int)(status));                          \
        return (status);                                                 \
    } while (0)

#define MMA8451_INVALID(where)                                           \
    do {                                                                 \
        uart_log_error("MMA8451: %s: invalid argument", (where));        \
        return HAL_ERROR;                                                \
    } while (0)

#define MMA8451_CHECK(where, call)                                       \
    do {                                                                 \
        HAL_StatusTypeDef _mma_status = (call);                          \
        if (_mma_status != HAL_OK) {                                     \
            MMA8451_FAIL((where), _mma_status);                          \
        }                                                                \
    } while (0)

#define MMA8451_DEFAULT_RANGE           MMA8451_RANGE_2G
#define MMA8451_DEFAULT_ODR             MMA8451_ODR_100HZ
#define MMA8451_DEFAULT_OVERSAMPLING    MMA8451_OS_HIGH_RESOLUTION
#define MMA8451_DEFAULT_LNOISE          0
#define MMA8451_USE_DRDY_INTERRUPT      0

static uint8_t mma8451_address = MMA8451_ADDR_DEFAULT;
static mma8451_range_t mma8451_range = MMA8451_DEFAULT_RANGE;
static mma8451_odr_t mma8451_odr = MMA8451_DEFAULT_ODR;

HAL_StatusTypeDef mma8451_read_reg(uint8_t reg, uint8_t *value)
{
    HAL_StatusTypeDef status;

    if (value == NULL) {
        MMA8451_INVALID("mma8451_read_reg");
    }

    status = HAL_I2C_Mem_Read(MMA8451_I2C_HANDLE,
                              (uint16_t)(mma8451_address << 1),
                              reg,
                              I2C_MEMADD_SIZE_8BIT,
                              value,
                              1U,
                              MMA8451_I2C_TIMEOUT_MS);
    if (status != HAL_OK) {
        MMA8451_FAIL("I2C register read", status);
    }

    return HAL_OK;
}

HAL_StatusTypeDef mma8451_write_reg(uint8_t reg, uint8_t value)
{
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Write(MMA8451_I2C_HANDLE,
                               (uint16_t)(mma8451_address << 1),
                               reg,
                               I2C_MEMADD_SIZE_8BIT,
                               &value,
                               1U,
                               MMA8451_I2C_TIMEOUT_MS);
    if (status != HAL_OK) {
        MMA8451_FAIL("I2C register write", status);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef mma8451_read_regs(uint8_t reg, uint8_t *data,
                                           uint16_t length)
{
    HAL_StatusTypeDef status;

    if (data == NULL) {
        MMA8451_INVALID("mma8451_read_regs");
    }

    status = HAL_I2C_Mem_Read(MMA8451_I2C_HANDLE,
                              (uint16_t)(mma8451_address << 1),
                              reg,
                              I2C_MEMADD_SIZE_8BIT,
                              data,
                              length,
                              MMA8451_I2C_TIMEOUT_MS);
    if (status != HAL_OK) {
        MMA8451_FAIL("I2C burst read", status);
    }

    return HAL_OK;
}

/* Put the device in STANDBY before modifying configuration registers.
 * The original CTRL_REG1 value is returned so it can be restored afterwards. */
static HAL_StatusTypeDef mma8451_enter_standby(uint8_t *ctrl_reg1)
{
    MMA8451_CHECK("read CTRL_REG1",
                  mma8451_read_reg(MMA8451_REG_CTRL_REG1, ctrl_reg1));

    MMA8451_CHECK("enter standby",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG1,
                                    (uint8_t)(*ctrl_reg1 &
                                              (uint8_t)~MMA8451_CTRL_REG1_ACTIVE)));

    return HAL_OK;
}

HAL_StatusTypeDef mma8451_init(uint8_t address)
{
    HAL_StatusTypeDef status;
    uint8_t who_am_i = 0U;
    uint8_t ctrl_reg2 = 0U;
    uint8_t ctrl_reg1;
    uint32_t start;

    mma8451_address = address;
    mma8451_range = MMA8451_DEFAULT_RANGE;
    mma8451_odr = MMA8451_DEFAULT_ODR;

    MMA8451_CHECK("read WHO_AM_I",
                  mma8451_read_reg(MMA8451_REG_WHO_AM_I, &who_am_i));
    if (who_am_i != MMA8451_WHO_AM_I_VALUE) {
        uart_log_error("MMA8451: unexpected WHO_AM_I 0x%02X (expected 0x%02X)",
                       who_am_i, MMA8451_WHO_AM_I_VALUE);
        return HAL_ERROR;
    }

    /* Software reset. RST clears itself when the reset sequence is done. */
    MMA8451_CHECK("software reset",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG2,
                                    MMA8451_CTRL_REG2_RST));

    start = HAL_GetTick();
    do {
        /* Read directly here so a transient NACK during reset does not flood
         * the UART with one error message per retry. */
        status = HAL_I2C_Mem_Read(MMA8451_I2C_HANDLE,
                                  (uint16_t)(mma8451_address << 1),
                                  MMA8451_REG_CTRL_REG2,
                                  I2C_MEMADD_SIZE_8BIT,
                                  &ctrl_reg2,
                                  1U,
                                  MMA8451_I2C_TIMEOUT_MS);
        if ((status == HAL_OK) &&
            ((ctrl_reg2 & MMA8451_CTRL_REG2_RST) == 0U)) {
            break;
        }
    } while ((HAL_GetTick() - start) < MMA8451_RESET_TIMEOUT_MS);

    if (status != HAL_OK) {
        MMA8451_FAIL("reset poll", status);
    }
    if ((ctrl_reg2 & MMA8451_CTRL_REG2_RST) != 0U) {
        uart_log_error("MMA8451: reset timed out after %u ms",
                       MMA8451_RESET_TIMEOUT_MS);
        return HAL_TIMEOUT;
    }

    MMA8451_CHECK("disable active mode",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG1, 0x00U));

    MMA8451_CHECK("set range",
                  mma8451_write_reg(MMA8451_REG_XYZ_DATA_CFG,
                                    (uint8_t)MMA8451_DEFAULT_RANGE));

    ctrl_reg2 = (uint8_t)(((uint8_t)MMA8451_DEFAULT_OVERSAMPLING <<
                           MMA8451_CTRL_REG2_MODS_SHIFT) &
                          MMA8451_CTRL_REG2_MODS_MASK);
    MMA8451_CHECK("set oversampling",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG2, ctrl_reg2));

#if MMA8451_USE_DRDY_INTERRUPT
    MMA8451_CHECK("enable data-ready interrupt",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG4,
                                    MMA8451_CTRL_REG4_INT_EN_DRDY));
    MMA8451_CHECK("route data-ready to INT1",
                  mma8451_write_reg(MMA8451_REG_CTRL_REG5,
                                    MMA8451_CTRL_REG5_INT_CFG_DRDY));
#endif

    ctrl_reg1 = (uint8_t)(MMA8451_CTRL_REG1_ACTIVE |
                          (((uint8_t)MMA8451_DEFAULT_ODR <<
                            MMA8451_CTRL_REG1_DR_SHIFT) &
                           MMA8451_CTRL_REG1_DR_MASK));

#if MMA8451_DEFAULT_LNOISE
    /* Low-noise mode limits the usable range to +/-4 g. */
    if (mma8451_range != MMA8451_RANGE_8G) {
        ctrl_reg1 |= MMA8451_CTRL_REG1_LNOISE;
    }
#endif

    status = mma8451_write_reg(MMA8451_REG_CTRL_REG1, ctrl_reg1);
    if (status != HAL_OK) {
        return status;
    }

    uart_log_info("MMA8451: initialized (addr=0x%02X, range=%ug)",
                  address,
                  (unsigned)(2U << (unsigned)mma8451_range));

    status = mma8451_set_data_rate(MMA8451_ODR_100HZ) != HAL_OK;
    if (status != HAL_OK) {
        uart_log_error("setting data rate failed");
        return status;
    }

    return HAL_OK;
}

static int16_t mma8451_parse_axis(uint8_t msb, uint8_t lsb)
{
    /* The 14-bit sample is left aligned in the 16-bit output registers:
     * MSB[7:0] = D13..D6, LSB[7:2] = D5..D0. */
    uint16_t value = (uint16_t)(((uint16_t)msb << 8) | lsb) >> 2;

    if ((value & 0x2000U) != 0U) {
        value |= 0xC000U;
    }

    return (int16_t)value;
}

HAL_StatusTypeDef mma8451_read_raw(mma8451_raw_t *raw)
{
    HAL_StatusTypeDef status;
    uint8_t buffer[6];

    if (raw == NULL) {
        MMA8451_INVALID("mma8451_read_raw");
    }

    /* Reading OUT_X_MSB..OUT_Z_LSB in one transaction clears the data-ready
     * flags and keeps the three axes from the same sample. */
    status = mma8451_read_regs(MMA8451_REG_OUT_X_MSB, buffer, sizeof(buffer));
    if (status != HAL_OK) {
        return status;
    }

    raw->x = mma8451_parse_axis(buffer[0], buffer[1]);
    raw->y = mma8451_parse_axis(buffer[2], buffer[3]);
    raw->z = mma8451_parse_axis(buffer[4], buffer[5]);

    return HAL_OK;
}

static float mma8451_counts_per_g(mma8451_range_t range)
{
    switch (range) {
    case MMA8451_RANGE_4G:
        return 2048.0f;
    case MMA8451_RANGE_8G:
        return 1024.0f;
    case MMA8451_RANGE_2G:
    default:
        return 4096.0f;
    }
}

HAL_StatusTypeDef mma8451_read_accel(mma8451_accel_t *accel)
{
    HAL_StatusTypeDef status;
    mma8451_raw_t raw;
    float scale;

    if (accel == NULL) {
        MMA8451_INVALID("mma8451_read_accel");
    }

    status = mma8451_read_raw(&raw);
    if (status != HAL_OK) {
        return status;
    }

    scale = 1.0f / mma8451_counts_per_g(mma8451_range);

    accel->x = (float)raw.x * scale;
    accel->y = (float)raw.y * scale;
    accel->z = (float)raw.z * scale;

    return HAL_OK;
}

HAL_StatusTypeDef mma8451_data_ready(uint8_t *ready)
{
    HAL_StatusTypeDef status;
    uint8_t status_reg = 0U;

    if (ready == NULL) {
        MMA8451_INVALID("mma8451_data_ready");
    }

    status = mma8451_read_reg(MMA8451_REG_STATUS, &status_reg);
    if (status != HAL_OK) {
        return status;
    }

    *ready = ((status_reg & MMA8451_STATUS_ZYXDR) != 0U) ? 1U : 0U;

    return HAL_OK;
}

HAL_StatusTypeDef mma8451_set_range(mma8451_range_t range)
{
    HAL_StatusTypeDef status;
    HAL_StatusTypeDef restore_status;
    uint8_t ctrl_reg1 = 0U;
    uint8_t data_cfg = 0U;

    if ((range != MMA8451_RANGE_2G) &&
        (range != MMA8451_RANGE_4G) &&
        (range != MMA8451_RANGE_8G)) {
        uart_log_error("MMA8451: set_range: invalid range %d", (int)range);
        return HAL_ERROR;
    }

    status = mma8451_enter_standby(&ctrl_reg1);
    if (status != HAL_OK) {
        return status;
    }

    status = mma8451_read_reg(MMA8451_REG_XYZ_DATA_CFG, &data_cfg);
    if (status != HAL_OK) {
        return status;
    }

    data_cfg = (uint8_t)((data_cfg & (uint8_t)~MMA8451_XYZ_DATA_CFG_FS_MASK) |
                         ((uint8_t)range << MMA8451_XYZ_DATA_CFG_FS_SHIFT));

    status = mma8451_write_reg(MMA8451_REG_XYZ_DATA_CFG, data_cfg);
    if (status == HAL_OK) {
        mma8451_range = range;
    }

    restore_status = mma8451_write_reg(MMA8451_REG_CTRL_REG1, ctrl_reg1);
    if (status == HAL_OK) {
        status = restore_status;
    }

    return status;
}

HAL_StatusTypeDef mma8451_set_data_rate(mma8451_odr_t odr)
{
    HAL_StatusTypeDef status;
    uint8_t ctrl_reg1 = 0U;

    if ((uint8_t)odr > (uint8_t)MMA8451_ODR_1_56HZ) {
        uart_log_error("MMA8451: set_data_rate: invalid ODR %d", (int)odr);
        return HAL_ERROR;
    }

    status = mma8451_enter_standby(&ctrl_reg1);
    if (status != HAL_OK) {
        return status;
    }

    ctrl_reg1 = (uint8_t)((ctrl_reg1 & (uint8_t)~MMA8451_CTRL_REG1_DR_MASK) |
                          (((uint8_t)odr << MMA8451_CTRL_REG1_DR_SHIFT) &
                           MMA8451_CTRL_REG1_DR_MASK));

    status = mma8451_write_reg(MMA8451_REG_CTRL_REG1, ctrl_reg1);
    if (status == HAL_OK) {
        mma8451_odr = odr;
    }

    return status;
}
