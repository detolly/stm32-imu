#ifndef MMA8451_H
#define MMA8451_H

#include <stdint.h>
#include "stm32f7xx_hal.h"

/* 7-bit addresses; the driver shifts left by one for the HAL. */
#define MMA8451_ADDR_SA0_LOW    0x1CU
#define MMA8451_ADDR_SA0_HIGH   0x1DU
#define MMA8451_ADDR_DEFAULT    MMA8451_ADDR_SA0_LOW

#define MMA8451_WHO_AM_I_VALUE  0x1AU

#define MMA8451_I2C_TIMEOUT_MS  100U
#define MMA8451_RESET_TIMEOUT_MS 100U

/* Register map (F_MODE = 0). */
typedef enum {
    MMA8451_REG_STATUS           = 0x00, /* F_STATUS when FIFO is enabled */
    MMA8451_REG_OUT_X_MSB        = 0x01,
    MMA8451_REG_OUT_X_LSB        = 0x02,
    MMA8451_REG_OUT_Y_MSB        = 0x03,
    MMA8451_REG_OUT_Y_LSB        = 0x04,
    MMA8451_REG_OUT_Z_MSB        = 0x05,
    MMA8451_REG_OUT_Z_LSB        = 0x06,
    MMA8451_REG_F_SETUP          = 0x09,
    MMA8451_REG_TRIG_CFG         = 0x0A,
    MMA8451_REG_SYSMOD           = 0x0B,
    MMA8451_REG_INT_SOURCE       = 0x0C,
    MMA8451_REG_WHO_AM_I         = 0x0D,
    MMA8451_REG_XYZ_DATA_CFG     = 0x0E,
    MMA8451_REG_HP_FILTER_CUTOFF = 0x0F,
    MMA8451_REG_PL_STATUS        = 0x10,
    MMA8451_REG_PL_CFG           = 0x11,
    MMA8451_REG_PL_COUNT         = 0x12,
    MMA8451_REG_PL_BF_ZCOMP      = 0x13,
    MMA8451_REG_P_L_THS          = 0x14,
    MMA8451_REG_FF_MT_CFG        = 0x15,
    MMA8451_REG_FF_MT_SRC        = 0x16,
    MMA8451_REG_FF_MT_THS        = 0x17,
    MMA8451_REG_FF_MT_COUNT      = 0x18,
    MMA8451_REG_TRANSIENT_CFG    = 0x1D,
    MMA8451_REG_TRANSIENT_SRC    = 0x1E,
    MMA8451_REG_TRANSIENT_THS    = 0x1F,
    MMA8451_REG_TRANSIENT_COUNT  = 0x20,
    MMA8451_REG_PULSE_CFG        = 0x21,
    MMA8451_REG_PULSE_SRC        = 0x22,
    MMA8451_REG_PULSE_THSX       = 0x23,
    MMA8451_REG_PULSE_THSY       = 0x24,
    MMA8451_REG_PULSE_THSZ       = 0x25,
    MMA8451_REG_PULSE_TMLT       = 0x26,
    MMA8451_REG_PULSE_LTCY       = 0x27,
    MMA8451_REG_PULSE_WIND       = 0x28,
    MMA8451_REG_ASLP_COUNT       = 0x29,
    MMA8451_REG_CTRL_REG1        = 0x2A,
    MMA8451_REG_CTRL_REG2        = 0x2B,
    MMA8451_REG_CTRL_REG3        = 0x2C,
    MMA8451_REG_CTRL_REG4        = 0x2D,
    MMA8451_REG_CTRL_REG5        = 0x2E,
    MMA8451_REG_OFF_X            = 0x2F,
    MMA8451_REG_OFF_Y            = 0x30,
    MMA8451_REG_OFF_Z            = 0x31
} mma8451_reg_t;

#define MMA8451_STATUS_ZYXOW        (1U << 7)
#define MMA8451_STATUS_ZOW          (1U << 6)
#define MMA8451_STATUS_YOW          (1U << 5)
#define MMA8451_STATUS_XOW          (1U << 4)
#define MMA8451_STATUS_ZYXDR        (1U << 3)
#define MMA8451_STATUS_ZDR          (1U << 2)
#define MMA8451_STATUS_YDR          (1U << 1)
#define MMA8451_STATUS_XDR          (1U << 0)

#define MMA8451_F_SETUP_F_MODE_SHIFT    6
#define MMA8451_F_SETUP_F_MODE_MASK     (0x03U << MMA8451_F_SETUP_F_MODE_SHIFT)
#define MMA8451_F_SETUP_F_WMRK_MASK     0x3FU

#define MMA8451_SYSMOD_SYSMOD_MASK      0x03U
#define MMA8451_SYSMOD_STANDBY          0x00U
#define MMA8451_SYSMOD_WAKE             0x01U
#define MMA8451_SYSMOD_SLEEP            0x02U

#define MMA8451_INT_SOURCE_ASLP         (1U << 7)
#define MMA8451_INT_SOURCE_FIFO         (1U << 6)
#define MMA8451_INT_SOURCE_TRANS        (1U << 5)
#define MMA8451_INT_SOURCE_LNDPRT       (1U << 4)
#define MMA8451_INT_SOURCE_PULSE        (1U << 3)
#define MMA8451_INT_SOURCE_FF_MT        (1U << 2)
#define MMA8451_INT_SOURCE_DRDY         (1U << 0)

#define MMA8451_XYZ_DATA_CFG_HPF_OUT    (1U << 4)
#define MMA8451_XYZ_DATA_CFG_FS_SHIFT   0
#define MMA8451_XYZ_DATA_CFG_FS_MASK    (0x03U << MMA8451_XYZ_DATA_CFG_FS_SHIFT)

#define MMA8451_CTRL_REG1_ASLP_RATE_SHIFT  6
#define MMA8451_CTRL_REG1_ASLP_RATE_MASK   (0x03U << MMA8451_CTRL_REG1_ASLP_RATE_SHIFT)
#define MMA8451_CTRL_REG1_DR_SHIFT         3
#define MMA8451_CTRL_REG1_DR_MASK          (0x07U << MMA8451_CTRL_REG1_DR_SHIFT)
#define MMA8451_CTRL_REG1_LNOISE           (1U << 2)
#define MMA8451_CTRL_REG1_F_READ           (1U << 1)
#define MMA8451_CTRL_REG1_ACTIVE           (1U << 0)

#define MMA8451_CTRL_REG2_ST               (1U << 7)
#define MMA8451_CTRL_REG2_RST              (1U << 6)
#define MMA8451_CTRL_REG2_SMODS_SHIFT      4
#define MMA8451_CTRL_REG2_SMODS_MASK       (0x03U << MMA8451_CTRL_REG2_SMODS_SHIFT)
#define MMA8451_CTRL_REG2_SLPE             (1U << 2)
#define MMA8451_CTRL_REG2_MODS_SHIFT       0
#define MMA8451_CTRL_REG2_MODS_MASK        (0x03U << MMA8451_CTRL_REG2_MODS_SHIFT)

#define MMA8451_CTRL_REG3_FIFO_GATE        (1U << 7)
#define MMA8451_CTRL_REG3_WAKE_TRANS       (1U << 6)
#define MMA8451_CTRL_REG3_WAKE_LNDPRT      (1U << 5)
#define MMA8451_CTRL_REG3_WAKE_PULSE       (1U << 4)
#define MMA8451_CTRL_REG3_WAKE_FF_MT       (1U << 3)
#define MMA8451_CTRL_REG3_IPOL             (1U << 1)
#define MMA8451_CTRL_REG3_PP_OD            (1U << 0)

#define MMA8451_CTRL_REG4_INT_EN_ASLP      (1U << 7)
#define MMA8451_CTRL_REG4_INT_EN_FIFO      (1U << 6)
#define MMA8451_CTRL_REG4_INT_EN_TRANS     (1U << 5)
#define MMA8451_CTRL_REG4_INT_EN_LNDPRT    (1U << 4)
#define MMA8451_CTRL_REG4_INT_EN_PULSE     (1U << 3)
#define MMA8451_CTRL_REG4_INT_EN_FF_MT     (1U << 2)
#define MMA8451_CTRL_REG4_INT_EN_DRDY      (1U << 0)

/* 0 = INT2, 1 = INT1 */
#define MMA8451_CTRL_REG5_INT_CFG_ASLP     (1U << 7)
#define MMA8451_CTRL_REG5_INT_CFG_FIFO     (1U << 6)
#define MMA8451_CTRL_REG5_INT_CFG_TRANS    (1U << 5)
#define MMA8451_CTRL_REG5_INT_CFG_LNDPRT   (1U << 4)
#define MMA8451_CTRL_REG5_INT_CFG_PULSE    (1U << 3)
#define MMA8451_CTRL_REG5_INT_CFG_FF_MT    (1U << 2)
#define MMA8451_CTRL_REG5_INT_CFG_DRDY     (1U << 0)

typedef enum {
    MMA8451_RANGE_2G = 0x00,
    MMA8451_RANGE_4G = 0x01,
    MMA8451_RANGE_8G = 0x02
} mma8451_range_t;

typedef enum {
    MMA8451_ODR_800HZ   = 0x00,
    MMA8451_ODR_400HZ   = 0x01,
    MMA8451_ODR_200HZ   = 0x02,
    MMA8451_ODR_100HZ   = 0x03,
    MMA8451_ODR_50HZ    = 0x04,
    MMA8451_ODR_12_5HZ  = 0x05,
    MMA8451_ODR_6_25HZ  = 0x06,
    MMA8451_ODR_1_56HZ  = 0x07
} mma8451_odr_t;

typedef enum {
    MMA8451_OS_NORMAL               = 0x00,
    MMA8451_OS_LOW_NOISE_LOW_POWER  = 0x01,
    MMA8451_OS_HIGH_RESOLUTION      = 0x02,
    MMA8451_OS_LOW_POWER            = 0x03
} mma8451_oversampling_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mma8451_raw_t;

typedef struct {
    float x;                    /* g */
    float y;                    /* g */
    float z;                    /* g */
} mma8451_accel_t;

HAL_StatusTypeDef mma8451_init(uint8_t address);

HAL_StatusTypeDef mma8451_read_raw(mma8451_raw_t *raw);
HAL_StatusTypeDef mma8451_read_accel(mma8451_accel_t *accel);
HAL_StatusTypeDef mma8451_data_ready(uint8_t *ready);

HAL_StatusTypeDef mma8451_read_reg(uint8_t reg, uint8_t *value);
HAL_StatusTypeDef mma8451_write_reg(uint8_t reg, uint8_t value);

HAL_StatusTypeDef mma8451_set_range(mma8451_range_t range);
HAL_StatusTypeDef mma8451_set_data_rate(mma8451_odr_t odr);

#endif /* MMA8451_H */
