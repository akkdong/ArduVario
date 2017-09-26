/*
 $License:
    Copyright (C) 2011-2012 InvenSense Corporation, All Rights Reserved.
    See included License.txt for License information.
 $
 */
/**
 *  @addtogroup  DRIVERS Sensor Driver Layer
 *  @brief       Hardware drivers to communicate with sensors via I2C.
 *
 *  @{
 *      @file       inv_mpu.c
 *      @brief      An I2C-based driver for Invensense gyroscopes.
 *      @details    This driver currently works for the following devices:
 *                  MPU6050
 *                  MPU6500
 *                  MPU9150 (or MPU6050 w/ AK8975 on the auxiliary bus)
 *                  MPU9250 (or MPU6500 w/ AK8963 on the auxiliary bus)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "inv_mpu.h"
#include "inv_dmp_uncompress.h"

/* The following functions must be defined for this platform:
 * i2c_write(unsigned char slave_addr, unsigned char reg_addr,
 *      unsigned char length, unsigned char const *data)
 * i2c_read(unsigned char slave_addr, unsigned char reg_addr,
 *      unsigned char length, unsigned char *data)
 * delay_ms(unsigned long num_ms)
 * get_ms(unsigned long *count)
 * reg_int_cb(void (*cb)(void), unsigned char port, unsigned char pin)
 * labs(long x)
 * fabsf(float x)
 * min(int a, int b)
 */

#define i2c_write   !I2Cdev::writeBytes
#define i2c_read    !I2Cdev::readBytes
#define delay_ms    delay

//static inline int reg_int_cb(struct int_param_s *int_param)
static inline void reg_int_cb(struct int_param_s *int_param)
{
}
#define inv_min(a,b) ((a<b)?a:b)

#if !defined MPU6050 && !defined MPU9150 && !defined MPU6500 && !defined MPU9250
#error  Which gyro are you using? Define MPUxxxx in your compiler options.
#endif

/* Time for some messy macro work. =]
 * #define MPU9150
 * is equivalent to..
 * #define MPU6050
 * #define AK8975_SECONDARY
 *
 * #define MPU9250
 * is equivalent to..
 * #define MPU6500
 * #define AK8963_SECONDARY
 */
#if defined MPU9150
#ifndef MPU6050
#define MPU6050
#endif                          /* #ifndef MPU6050 */
#if defined AK8963_SECONDARY
#error "MPU9150 and AK8963_SECONDARY cannot both be defined."
#elif !defined AK8975_SECONDARY /* #if defined AK8963_SECONDARY */
#define AK8975_SECONDARY
#endif                          /* #if defined AK8963_SECONDARY */
#elif defined MPU9250           /* #if defined MPU9150 */
#ifndef MPU6500
#define MPU6500
#endif                          /* #ifndef MPU6500 */
#if defined AK8975_SECONDARY
#error "MPU9250 and AK8975_SECONDARY cannot both be defined."
#elif !defined AK8963_SECONDARY /* #if defined AK8975_SECONDARY */
#define AK8963_SECONDARY
#endif                          /* #if defined AK8975_SECONDARY */
#endif                          /* #if defined MPU9150 */

#if defined AK8975_SECONDARY || defined AK8963_SECONDARY
#define AK89xx_SECONDARY
#else
/* #warning "No compass = less profit for Invensense. Lame." */
#endif


#if defined MPU6050
#define GYRO_REG_WHO_AM_I (0x75)
#define GYRO_REG_RATE_DIV (0x19)
#define GYRO_REG_LPF (0x1A)
#define GYRO_REG_PROD_ID (0x0C)
#define GYRO_REG_USER_CTRL (0x6A)
#define GYRO_REG_FIFO_EN (0x23)
#define GYRO_REG_GYRO_CFG (0x1B)
#define GYRO_REG_ACCEL_CFG (0x1C)
#define GYRO_REG_MOTION_THR (0x1F)
#define GYRO_REG_MOTION_DUR (0x20)
#define GYRO_REG_FIFO_COUNT_H (0x72)
#define GYRO_REG_FIFO_R_W (0x74)
#define GYRO_REG_RAW_GYRO (0x43)
#define GYRO_REG_RAW_ACCEL (0x3B)
#define GYRO_REG_TEMP (0x41)
#define GYRO_REG_INT_ENABLE (0x38)
#define GYRO_REG_DMP_INT_STATUS (0x39)
#define GYRO_REG_INT_STATUS (0x3A)
#define GYRO_REG_PWR_MGMT_1 (0x6B)
#define GYRO_REG_PWR_MGMT_2 (0x6C)
#define GYRO_REG_INT_PIN_CFG (0x37)
#define GYRO_REG_MEM_R_W (0x6F)
#define GYRO_REG_ACCEL_OFFS (0x06)
#define GYRO_REG_I2C_MST (0x24)
#define GYRO_REG_BANK_SEL (0x6D)
#define GYRO_REG_MEM_START_ADDR (0x6E)
#define GYRO_REG_PRGM_START_H (0x70)
#ifdef AK89xx_SECONDARY
#define GYRO_REG_RAW_COMPASS (0x49)
#define GYRO_REG_YG_OFFS_TC (0x01)
#define GYRO_REG_S0_ADDR (0x25)
#define GYRO_REG_S0_REG (0x26)
#define GYRO_REG_S0_CTRL (0x27)
#define GYRO_REG_S1_ADDR (0x28)
#define GYRO_REG_S1_REG (0x29)
#define GYRO_REG_S1_CTRL (0x2A)
#define GYRO_REG_S4_CTRL (0x34)
#define GYRO_REG_S0_DO (0x63)
#define GYRO_REG_S1_DO (0x64)
#define GYRO_REG_I2C_DELAY_CTRL (0x67)
#endif
#define GYRO_HW_ADDR (0x68)
#define GYRO_HW_MAX_FIFO 1024
#define GYRO_HW_NUM_REG 118
#define GYRO_HW_TEMP_SENS 340
#define GYRO_HW_TEMP_OFFSET (-521)
#define GYRO_HW_BANK_SIZE 256
#if defined AK89xx_SECONDARY
#define GYRO_HW_COMPASS_FSR AK89xx_FSR
#endif
#define GYRO_TEST_GYRO_SENS (32768/250)
#define GYRO_TEST_ACCEL_SENS (32768/16)
#define GYRO_TEST_REG_RATE_DIV 0
#define GYRO_TEST_REG_LPF 1
#define GYRO_TEST_REG_GYRO_FSR 0
#define GYRO_TEST_REG_ACCEL_FSR (0x18)
#define GYRO_TEST_WAIT_MS 50
#define GYRO_TEST_PACKET_THRESH 5
#define GYRO_TEST_MIN_DPS (10.f)
#define GYRO_TEST_MAX_DPS (105.f)
#define GYRO_TEST_MAX_GYRO_VAR (0.14f)
#define GYRO_TEST_MIN_G (0.3f)
#define GYRO_TEST_MAX_G (0.95)
#define GYRO_TEST_MAX_ACCEL_VAR (0.14f)
#elif defined MPU6500
#define GYRO_REG_WHO_AM_I (0x75)
#define GYRO_REG_RATE_DIV (0x19)
#define GYRO_REG_LPF (0x1A)
#define GYRO_REG_PROD_ID (0x0C)
#define GYRO_REG_USER_CTRL (0x6A)
#define GYRO_REG_FIFO_EN (0x23)
#define GYRO_REG_GYRO_CFG (0x1B)
#define GYRO_REG_ACCEL_CFG (0x1C)
#define GYRO_REG_ACCEL_CFG2 (0x1D)
#define GYRO_REG_LP_ACCEL_ODR (0x1E)
#define GYRO_REG_MOTION_THR (0x1F)
#define GYRO_REG_MOTION_DUR (0x20)
#define GYRO_REG_FIFO_COUNT_H (0x72)
#define GYRO_REG_FIFO_R_W (0x74)
#define GYRO_REG_RAW_GYRO (0x43)
#define GYRO_REG_RAW_ACCEL (0x3B)
#define GYRO_REG_TEMP (0x41)
#define GYRO_REG_INT_ENABLE (0x38)
#define GYRO_REG_DMP_INT_STATUS (0x39)
#define GYRO_REG_INT_STATUS (0x3A)
#define GYRO_REG_ACCEL_INTEL (0x69)
#define GYRO_REG_PWR_MGMT_1 (0x6B)
#define GYRO_REG_PWR_MGMT_2 (0x6C)
#define GYRO_REG_INT_PIN_CFG (0x37)
#define GYRO_REG_MEM_R_W (0x6F)
#define GYRO_REG_ACCEL_OFFS (0x77)
#define GYRO_REG_I2C_MST (0x24)
#define GYRO_REG_BANK_SEL (0x6D)
#define GYRO_REG_MEM_START_ADDR (0x6E)
#define GYRO_REG_PRGM_START_H (0x70)
#ifdef AK89xx_SECONDARY
#define GYRO_REG_RAW_COMPASS (0x49)
#define GYRO_REG_YG_OFFS_TC (0x01)
#define GYRO_REG_S0_ADDR (0x25)
#define GYRO_REG_S0_REG (0x26)
#define GYRO_REG_S0_CTRL (0x27)
#define GYRO_REG_S1_ADDR (0x28)
#define GYRO_REG_S1_REG (0x29)
#define GYRO_REG_S1_CTRL (0x2A)
#define GYRO_REG_S4_CTRL (0x34)
#define GYRO_REG_S0_DO (0x63)
#define GYRO_REG_S1_DO (0x64)
#define GYRO_REG_I2C_DELAY_CTRL (0x67)
#endif
#define GYRO_HW_ADDR (0x68)
#define GYRO_HW_MAX_FIFO 1024
#define GYRO_HW_NUM_REG 128
#define GYRO_HW_TEMP_SENS 321
#define GYRO_HW_TEMP_OFFSET 0
#define GYRO_HW_BANK_SIZE 256
#if defined AK89xx_SECONDARY
#define GYRO_HW_COMPASS_FSR AK89xx_FSR
#endif
#define GYRO_TEST_GYRO_SENS (32768/250)
#define GYRO_TEST_ACCEL_SENS (32768/16)
#define GYRO_TEST_REG_RATE_DIV 0
#define GYRO_TEST_REG_LPF 1
#define GYRO_TEST_REG_GYRO_FSR 0
#define GYRO_TEST_REG_ACCEL_FSR (0x18)
#define GYRO_TEST_WAIT_MS 50
#define GYRO_TEST_PACKET_THRESH 5
#define GYRO_TEST_MIN_DPS (10.f)
#define GYRO_TEST_MAX_DPS (105.f)
#define GYRO_TEST_MAX_GYRO_VAR (0.14f)
#define GYRO_TEST_MIN_G (0.3f)
#define GYRO_TEST_MAX_G (0.95f)
#define GYRO_TEST_MAX_ACCEL_VAR (0.14f)
#endif


static int set_int_enable(unsigned char enable);

/* When entering motion interrupt mode, the driver keeps track of the
 * previous state so that it can be restored at a later time.
 * TODO: This is tacky. Fix it.
 */
struct motion_int_cache_s {
    unsigned short gyro_fsr;
    unsigned char accel_fsr;
    unsigned short lpf;
    unsigned short sample_rate;
    unsigned char sensors_on;
    unsigned char fifo_sensors;
    unsigned char dmp_on;
};

/* Cached chip configuration data.
 * TODO: A lot of these can be handled with a bitmask.
 */
struct chip_cfg_s {
    /* Matches gyro_cfg >> 3 & 0x03 */
    unsigned char gyro_fsr;
    /* Matches accel_cfg >> 3 & 0x03 */
    unsigned char accel_fsr;
    /* Enabled sensors. Uses same masks as fifo_en, NOT pwr_mgmt_2. */
    unsigned char sensors;
    /* Matches config register. */
    unsigned char lpf;
    unsigned char clk_src;
    /* Sample rate, NOT rate divider. */
    unsigned short sample_rate;
    /* Matches fifo_en register. */
    unsigned char fifo_enable;
    /* Matches int enable register. */
    unsigned char int_enable;
    /* 1 if devices on auxiliary I2C bus appear on the primary. */
    unsigned char bypass_mode;
    /* 1 if half-sensitivity.
     * NOTE: This doesn't belong here, but everything else in hw_s is const,
     * and this allows us to save some precious RAM.
     */
    unsigned char accel_half;
    /* 1 if device in low-power accel-only mode. */
    unsigned char lp_accel_mode;
    /* 1 if interrupts are only triggered on motion events. */
    unsigned char int_motion_only;
    struct motion_int_cache_s cache;
    /* 1 for active low interrupts. */
    unsigned char active_low_int;
    /* 1 for latched interrupts. */
    unsigned char latched_int;
    /* 1 if DMP is enabled. */
    unsigned char dmp_on;
    /* Ensures that DMP will only be loaded once. */
    unsigned char dmp_loaded;
    /* Sampling rate used when DMP is enabled. */
    unsigned short dmp_sample_rate;
#ifdef AK89xx_SECONDARY
    /* Compass sample rate. */
    unsigned short compass_sample_rate;
    unsigned char compass_addr;
    short mag_sens_adj[3];
#endif
};

/* Gyro driver state variables. */
struct gyro_state_s {
    const struct gyro_reg_s *reg;
    const struct hw_s *hw;
    struct chip_cfg_s chip_cfg;
    const struct test_s *test;
};

/* Filter configurations. */
enum lpf_e {
    INV_FILTER_256HZ_NOLPF2 = 0,
    INV_FILTER_188HZ,
    INV_FILTER_98HZ,
    INV_FILTER_42HZ,
    INV_FILTER_20HZ,
    INV_FILTER_10HZ,
    INV_FILTER_5HZ,
    INV_FILTER_2100HZ_NOLPF,
    NUM_FILTER
};

/* Full scale ranges. */
enum gyro_fsr_e {
    INV_FSR_250DPS = 0,
    INV_FSR_500DPS,
    INV_FSR_1000DPS,
    INV_FSR_2000DPS,
    NUM_GYRO_FSR
};

/* Full scale ranges. */
enum accel_fsr_e {
    INV_FSR_2G = 0,
    INV_FSR_4G,
    INV_FSR_8G,
    INV_FSR_16G,
    NUM_ACCEL_FSR
};

/* Clock sources. */
enum clock_sel_e {
    INV_CLK_INTERNAL = 0,
    INV_CLK_PLL,
    NUM_CLK
};

/* Low-power accel wakeup rates. */
enum lp_accel_rate_e {
#if defined MPU6050
    INV_LPA_1_25HZ,
    INV_LPA_5HZ,
    INV_LPA_20HZ,
    INV_LPA_40HZ
#elif defined MPU6500
    INV_LPA_0_3125HZ,
    INV_LPA_0_625HZ,
    INV_LPA_1_25HZ,
    INV_LPA_2_5HZ,
    INV_LPA_5HZ,
    INV_LPA_10HZ,
    INV_LPA_20HZ,
    INV_LPA_40HZ,
    INV_LPA_80HZ,
    INV_LPA_160HZ,
    INV_LPA_320HZ,
    INV_LPA_640HZ
#endif
};

#define BIT_I2C_MST_VDDIO   (0x80)
#define BIT_FIFO_EN         (0x40)
#define BIT_DMP_EN          (0x80)
#define BIT_FIFO_RST        (0x04)
#define BIT_DMP_RST         (0x08)
#define BIT_FIFO_OVERFLOW   (0x10)
#define BIT_DATA_RDY_EN     (0x01)
#define BIT_DMP_INT_EN      (0x02)
#define BIT_MOT_INT_EN      (0x40)
#define BITS_FSR            (0x18)
#define BITS_LPF            (0x07)
#define BITS_HPF            (0x07)
#define BITS_CLK            (0x07)
#define BIT_FIFO_SIZE_1024  (0x40)
#define BIT_FIFO_SIZE_2048  (0x80)
#define BIT_FIFO_SIZE_4096  (0xC0)
#define BIT_RESET           (0x80)
#define BIT_SLEEP           (0x40)
#define BIT_S0_DELAY_EN     (0x01)
#define BIT_S2_DELAY_EN     (0x04)
#define BITS_SLAVE_LENGTH   (0x0F)
#define BIT_SLAVE_BYTE_SW   (0x40)
#define BIT_SLAVE_GROUP     (0x10)
#define BIT_SLAVE_EN        (0x80)
#define BIT_I2C_READ        (0x80)
#define BITS_I2C_MASTER_DLY (0x1F)
#define BIT_AUX_IF_EN       (0x20)
#define BIT_ACTL            (0x80)
#define BIT_LATCH_EN        (0x20)
#define BIT_ANY_RD_CLR      (0x10)
#define BIT_BYPASS_EN       (0x02)
#define BITS_WOM_EN         (0xC0)
#define BIT_LPA_CYCLE       (0x20)
#define BIT_STBY_XA         (0x20)
#define BIT_STBY_YA         (0x10)
#define BIT_STBY_ZA         (0x08)
#define BIT_STBY_XG         (0x04)
#define BIT_STBY_YG         (0x02)
#define BIT_STBY_ZG         (0x01)
#define BIT_STBY_XYZA       (BIT_STBY_XA | BIT_STBY_YA | BIT_STBY_ZA)
#define BIT_STBY_XYZG       (BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG)

#if defined AK8975_SECONDARY
#define SUPPORTS_AK89xx_HIGH_SENS   (0x00)
#define AK89xx_FSR                  (9830)
#elif defined AK8963_SECONDARY
#define SUPPORTS_AK89xx_HIGH_SENS   (0x10)
#define AK89xx_FSR                  (4915)
#endif

#ifdef AK89xx_SECONDARY
#define AKM_REG_WHOAMI      (0x00)

#define AKM_REG_ST1         (0x02)
#define AKM_REG_HXL         (0x03)
#define AKM_REG_ST2         (0x09)

#define AKM_REG_CNTL        (0x0A)
#define AKM_REG_ASTC        (0x0C)
#define AKM_REG_ASAX        (0x10)
#define AKM_REG_ASAY        (0x11)
#define AKM_REG_ASAZ        (0x12)

#define AKM_DATA_READY      (0x01)
#define AKM_DATA_OVERRUN    (0x02)
#define AKM_OVERFLOW        (0x80)
#define AKM_DATA_ERROR      (0x40)

#define AKM_BIT_SELF_TEST   (0x40)

#define AKM_POWER_DOWN          (0x00 | SUPPORTS_AK89xx_HIGH_SENS)
#define AKM_SINGLE_MEASUREMENT  (0x01 | SUPPORTS_AK89xx_HIGH_SENS)
#define AKM_FUSE_ROM_ACCESS     (0x0F | SUPPORTS_AK89xx_HIGH_SENS)
#define AKM_MODE_SELF_TEST      (0x08 | SUPPORTS_AK89xx_HIGH_SENS)

#define AKM_WHOAMI      (0x48)
#endif


/* gyro structures */
struct gyro_state_s gyroState;
struct gyro_state_s *st = &gyroState;

/* useless : functionnality removed */ 
int mpu_select_device(int device)
{
  return 0;
}

/* useless : functionnality removed */
void mpu_init_structures()
{
  
}

#define MAX_PACKET_LENGTH (12)

#ifdef AK89xx_SECONDARY
static int setup_compass(void);
#define MAX_COMPASS_SAMPLE_RATE (100)
#endif

/**
 *  @brief      Enable/disable data ready interrupt.
 *  If the DMP is on, the DMP interrupt is enabled. Otherwise, the data ready
 *  interrupt is used.
 *  @param[in]  enable      1 to enable interrupt.
 *  @return     0 if successful.
 */
static int set_int_enable(unsigned char enable)
{
    unsigned char tmp;

    if (st->chip_cfg.dmp_on) {
        if (enable)
            tmp = BIT_DMP_INT_EN;
        else
            tmp = 0x00;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, &tmp))
            return -1;
        st->chip_cfg.int_enable = tmp;
    } else {
        if (!st->chip_cfg.sensors)
            return -1;
        if (enable && st->chip_cfg.int_enable)
            return 0;
        if (enable)
            tmp = BIT_DATA_RDY_EN;
        else
            tmp = 0x00;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, &tmp))
            return -1;
        st->chip_cfg.int_enable = tmp;
    }
    return 0;
}

/**
 *  @brief      Register dump for testing.
 *  @return     0 if successful.
 */
int mpu_reg_dump(void)
{
    unsigned char ii;
    unsigned char data;

    for (ii = 0; ii < GYRO_HW_NUM_REG; ii++) {
        if (ii == GYRO_REG_FIFO_R_W || ii == GYRO_REG_MEM_R_W)
            continue;
        if (i2c_read(GYRO_HW_ADDR, ii, 1, &data))
            return -1;
//      log_i("%#5x: %#5x\r\n", ii, data);
    }
    return 0;
}

/**
 *  @brief      Read from a single register.
 *  NOTE: The memory and FIFO read/write registers cannot be accessed.
 *  @param[in]  reg     Register address.
 *  @param[out] data    Register data.
 *  @return     0 if successful.
 */
int mpu_read_reg(unsigned char reg, unsigned char *data)
{
    if (reg == GYRO_REG_FIFO_R_W || reg == GYRO_REG_MEM_R_W)
        return -1;
    if (reg >= GYRO_HW_NUM_REG)
        return -1;
    return i2c_read(GYRO_HW_ADDR, reg, 1, data);
}

/**
 *  @brief      Initialize hardware.
 *  Initial configuration:\n
 *  Gyro FSR: +/- 2000DPS\n
 *  Accel FSR +/- 2G\n
 *  DLPF: 42Hz\n
 *  FIFO rate: 50Hz\n
 *  Clock source: Gyro PLL\n
 *  FIFO: Disabled.\n
 *  Data ready interrupt: Disabled, active low, unlatched.
 *  @param[in]  int_param   Platform-specific parameters to interrupt API.
 *  @return     0 if successful.
 */

int mpu_init(struct int_param_s *int_param)
{
    unsigned char data[6], rev;
    int errCode;

    /* Reset device. */
    data[0] = BIT_RESET;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, data))
        return -1;
    delay_ms(100);

    /* Wake up chip. */
    data[0] = 0x00;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, data))
        return -2;

#if defined MPU6050
    /* Check product revision. */
    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_ACCEL_OFFS, 6, data))
        return -3;
    rev = ((data[5] & 0x01) << 2) | ((data[3] & 0x01) << 1) |
        (data[1] & 0x01);

    if (rev) {
        /* Congrats, these parts are better. */
        if (rev == 1)
            st->chip_cfg.accel_half = 1;
        else if (rev == 2)
            st->chip_cfg.accel_half = 0;
        else {
#ifdef MPU_DEBUG
            Serial.print("Unsupported software product rev: "); Serial.println(rev);
#endif
            return -4;
        }
    } else {
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_PROD_ID, 1, data))
            return -5;
        rev = data[0] & 0x0F;
        if (!rev) {
#ifdef MPU_DEBUG
            Serial.println("Product ID read as 0 indicates device is either incompatible or an MPU3050");
#endif
            return -6;
        } else if (rev == 4) {
#ifdef MPU_DEBUG
            Serial.println("Half sensitivity part found.");
#endif
            st->chip_cfg.accel_half = 1;
        } else
            st->chip_cfg.accel_half = 0;
    }
#elif defined MPU6500
#define MPU6500_MEM_REV_ADDR    (0x17)
    if (mpu_read_mem(MPU6500_MEM_REV_ADDR, 1, &rev))
        return -7;
    if (rev == 0x1)
        st->chip_cfg.accel_half = 0;
    else {
#ifdef MPU_DEBUG
        Serial.print("Unsupported software product rev: "); Serial.println(rev);
#endif
        return -8;
    }

    /* MPU6500 shares 4kB of memory between the DMP and the FIFO. Since the
     * first 3kB are needed by the DMP, we'll use the last 1kB for the FIFO.
     */
    data[0] = BIT_FIFO_SIZE_1024 | 0x8;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_CFG2, 1, data))
        return -9;
#endif

    /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.bypass_mode = 0xFF;
#ifdef AK89xx_SECONDARY
    st->chip_cfg.compass_sample_rate = 0xFFFF;
#endif
    /* mpu_set_sensors always preserves this setting. */
    st->chip_cfg.clk_src = INV_CLK_PLL;
    /* Handled in next call to mpu_set_bypass. */
    st->chip_cfg.active_low_int = 1;
    st->chip_cfg.latched_int = 0;
    st->chip_cfg.int_motion_only = 0;
    st->chip_cfg.lp_accel_mode = 0;
    memset(&st->chip_cfg.cache, 0, sizeof(st->chip_cfg.cache));
    st->chip_cfg.dmp_on = 0;
    st->chip_cfg.dmp_loaded = 0;
    st->chip_cfg.dmp_sample_rate = 0;

    if (mpu_set_gyro_fsr(2000))
        return -10;
    if (mpu_set_accel_fsr(2))
        return -11;
    if (mpu_set_lpf(42))
        return -12;
    if (mpu_set_sample_rate(50))
        return -13;
    if (mpu_configure_fifo(0))
        return -14;

    if (int_param)
        reg_int_cb(int_param);

#ifdef AK89xx_SECONDARY
#ifdef MPU_DEBUG
    Serial.println("Setting up compass");
#endif
    errCode = setup_compass();
    if (errCode != 0) {
#ifdef MPU_DEBUG
       Serial.print("Setup compass failed: "); Serial.println(errCode); 
#endif
    }
    if (mpu_set_compass_sample_rate(10))
        return -15;
#else
    /* Already disabled by setup_compass. */
    if (mpu_set_bypass(0))
        return -16;
#endif

    mpu_set_sensors(0);
    return 0;
}

/**
 *  @brief      Enter low-power accel-only mode.
 *  In low-power accel mode, the chip goes to sleep and only wakes up to sample
 *  the accelerometer at one of the following frequencies:
 *  \n MPU6050: 1.25Hz, 5Hz, 20Hz, 40Hz
 *  \n MPU6500: 1.25Hz, 2.5Hz, 5Hz, 10Hz, 20Hz, 40Hz, 80Hz, 160Hz, 320Hz, 640Hz
 *  \n If the requested rate is not one listed above, the device will be set to
 *  the next highest rate. Requesting a rate above the maximum supported
 *  frequency will result in an error.
 *  \n To select a fractional wake-up frequency, round down the value passed to
 *  @e rate.
 *  @param[in]  rate        Minimum sampling rate, or zero to disable LP
 *                          accel mode.
 *  @return     0 if successful.
 */
int mpu_lp_accel_mode(unsigned char rate)
{
    unsigned char tmp[2];

    if (rate > 40)
        return -1;

    if (!rate) {
        mpu_set_int_latched(0);
        tmp[0] = 0;
        tmp[1] = BIT_STBY_XYZG;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 2, tmp))
            return -1;
        st->chip_cfg.lp_accel_mode = 0;
        return 0;
    }
    /* For LP accel, we automatically configure the hardware to produce latched
     * interrupts. In LP accel mode, the hardware cycles into sleep mode before
     * it gets a chance to deassert the interrupt pin; therefore, we shift this
     * responsibility over to the MCU.
     *
     * Any register read will clear the interrupt.
     */
    mpu_set_int_latched(1);
#if defined MPU6050
    tmp[0] = BIT_LPA_CYCLE;
    if (rate == 1) {
        tmp[1] = INV_LPA_1_25HZ;
        mpu_set_lpf(5);
    } else if (rate <= 5) {
        tmp[1] = INV_LPA_5HZ;
        mpu_set_lpf(5);
    } else if (rate <= 20) {
        tmp[1] = INV_LPA_20HZ;
        mpu_set_lpf(10);
    } else {
        tmp[1] = INV_LPA_40HZ;
        mpu_set_lpf(20);
    }
    tmp[1] = (tmp[1] << 6) | BIT_STBY_XYZG;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 2, tmp))
        return -1;
#elif defined MPU6500
    /* Set wake frequency. */
    if (rate == 1)
        tmp[0] = INV_LPA_1_25HZ;
    else if (rate == 2)
        tmp[0] = INV_LPA_2_5HZ;
    else if (rate <= 5)
        tmp[0] = INV_LPA_5HZ;
    else if (rate <= 10)
        tmp[0] = INV_LPA_10HZ;
    else if (rate <= 20)
        tmp[0] = INV_LPA_20HZ;
    else if (rate <= 40)
        tmp[0] = INV_LPA_40HZ;
    else if (rate <= 80)
        tmp[0] = INV_LPA_80HZ;
    else if (rate <= 160)
        tmp[0] = INV_LPA_160HZ;
    else if (rate <= 320)
        tmp[0] = INV_LPA_320HZ;
    else
        tmp[0] = INV_LPA_640HZ;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_LP_ACCEL_ODR, 1, tmp))
        return -1;
    tmp[0] = BIT_LPA_CYCLE;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, tmp))
        return -1;
#endif
    st->chip_cfg.sensors = INV_XYZ_ACCEL;
    st->chip_cfg.clk_src = 0;
    st->chip_cfg.lp_accel_mode = 1;
    mpu_configure_fifo(0);

    return 0;
}

/**
 *  @brief      Read raw gyro data directly from the registers.
 *  @param[out] data        Raw data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int mpu_get_gyro_reg(short *data, unsigned long *timestamp)
{
    unsigned char tmp[6];

    if (!(st->chip_cfg.sensors & INV_XYZ_GYRO))
        return -1;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_RAW_GYRO, 6, tmp))
        return -1;
    data[0] = (tmp[0] << 8) | tmp[1];
    data[1] = (tmp[2] << 8) | tmp[3];
    data[2] = (tmp[4] << 8) | tmp[5];
    if (timestamp)
        get_ms(timestamp);
    return 0;
}

/**
 *  @brief      Read raw accel data directly from the registers.
 *  @param[out] data        Raw data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int mpu_get_accel_reg(short *data, unsigned long *timestamp)
{
    unsigned char tmp[6];

    if (!(st->chip_cfg.sensors & INV_XYZ_ACCEL))
        return -1;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_RAW_ACCEL, 6, tmp))
        return -1;
    data[0] = (tmp[0] << 8) | tmp[1];
    data[1] = (tmp[2] << 8) | tmp[3];
    data[2] = (tmp[4] << 8) | tmp[5];
    if (timestamp)
        get_ms(timestamp);
    return 0;
}

/**
 *  @brief      Read temperature data directly from the registers.
 *  @param[out] data        Data in q16 format.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int mpu_get_temperature(long *data, unsigned long *timestamp)
{
    unsigned char tmp[2];
    short raw;

    if (!(st->chip_cfg.sensors))
        return -1;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_TEMP, 2, tmp))
        return -1;
    raw = (tmp[0] << 8) | tmp[1];
    if (timestamp)
        get_ms(timestamp);

    data[0] = (long)((35 + ((raw - (float)GYRO_HW_TEMP_OFFSET) / GYRO_HW_TEMP_SENS)) * 65536L);
    return 0;
}

/**
 *  @brief      Push biases to the accel bias registers.
 *  This function expects biases relative to the current sensor output, and
 *  these biases will be added to the factory-supplied values.
 *  @param[in]  accel_bias  New biases.
 *  @return     0 if successful.
 */
int mpu_set_accel_bias(const long *accel_bias)
{
    unsigned char data[6];
    short accel_hw[3];
    short got_accel[3];
    short fg[3];

    if (!accel_bias)
        return -1;
    if (!accel_bias[0] && !accel_bias[1] && !accel_bias[2])
        return 0;

    if (i2c_read(GYRO_HW_ADDR, 3, 3, data))
        return -1;
    fg[0] = ((data[0] >> 4) + 8) & 0xf;
    fg[1] = ((data[1] >> 4) + 8) & 0xf;
    fg[2] = ((data[2] >> 4) + 8) & 0xf;

    accel_hw[0] = (short)(accel_bias[0] * 2 / (64 + fg[0]));
    accel_hw[1] = (short)(accel_bias[1] * 2 / (64 + fg[1]));
    accel_hw[2] = (short)(accel_bias[2] * 2 / (64 + fg[2]));

    if (i2c_read(GYRO_HW_ADDR, 0x06, 6, data))
        return -1;

    got_accel[0] = ((short)data[0] << 8) | data[1];
    got_accel[1] = ((short)data[2] << 8) | data[3];
    got_accel[2] = ((short)data[4] << 8) | data[5];

    accel_hw[0] += got_accel[0];
    accel_hw[1] += got_accel[1];
    accel_hw[2] += got_accel[2];

    data[0] = (accel_hw[0] >> 8) & 0xff;
    data[1] = (accel_hw[0]) & 0xff;
    data[2] = (accel_hw[1] >> 8) & 0xff;
    data[3] = (accel_hw[1]) & 0xff;
    data[4] = (accel_hw[2] >> 8) & 0xff;
    data[5] = (accel_hw[2]) & 0xff;

    if (i2c_write(GYRO_HW_ADDR, 0x06, 6, data))
        return -1;
    return 0;
}

/**
 *  @brief  Reset FIFO read/write pointers.
 *  @return 0 if successful.
 */
int mpu_reset_fifo(void)
{
    unsigned char data;

    if (!(st->chip_cfg.sensors))
        return -1;

    data = 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, &data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, &data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &data))
        return -1;

    if (st->chip_cfg.dmp_on) {
        data = BIT_FIFO_RST | BIT_DMP_RST;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &data))
            return -1;
        delay_ms(50);
        data = BIT_DMP_EN | BIT_FIFO_EN;
        if (st->chip_cfg.sensors & INV_XYZ_COMPASS)
            data |= BIT_AUX_IF_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &data))
            return -1;
        if (st->chip_cfg.int_enable)
            data = BIT_DMP_INT_EN;
        else
            data = 0;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, &data))
            return -1;
        data = 0;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, &data))
            return -1;
    } else {
        data = BIT_FIFO_RST;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &data))
            return -1;
        if (st->chip_cfg.bypass_mode || !(st->chip_cfg.sensors & INV_XYZ_COMPASS))
            data = BIT_FIFO_EN;
        else
            data = BIT_FIFO_EN | BIT_AUX_IF_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &data))
            return -1;
        delay_ms(50);
        if (st->chip_cfg.int_enable)
            data = BIT_DATA_RDY_EN;
        else
            data = 0;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, &data))
            return -1;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, &st->chip_cfg.fifo_enable))
            return -1;
    }
    return 0;
}

/**
 *  @brief      Get the gyro full-scale range.
 *  @param[out] fsr Current full-scale range.
 *  @return     0 if successful.
 */
int mpu_get_gyro_fsr(unsigned short *fsr)
{
    switch (st->chip_cfg.gyro_fsr) {
    case INV_FSR_250DPS:
        fsr[0] = 250;
        break;
    case INV_FSR_500DPS:
        fsr[0] = 500;
        break;
    case INV_FSR_1000DPS:
        fsr[0] = 1000;
        break;
    case INV_FSR_2000DPS:
        fsr[0] = 2000;
        break;
    default:
        fsr[0] = 0;
        break;
    }
    return 0;
}

/**
 *  @brief      Set the gyro full-scale range.
 *  @param[in]  fsr Desired full-scale range.
 *  @return     0 if successful.
 */
int mpu_set_gyro_fsr(unsigned short fsr)
{
    unsigned char data;

    if (!(st->chip_cfg.sensors))
        return -1;

    switch (fsr) {
    case 250:
        data = INV_FSR_250DPS << 3;
        break;
    case 500:
        data = INV_FSR_500DPS << 3;
        break;
    case 1000:
        data = INV_FSR_1000DPS << 3;
        break;
    case 2000:
        data = INV_FSR_2000DPS << 3;
        break;
    default:
        return -1;
    }

    if (st->chip_cfg.gyro_fsr == (data >> 3))
        return 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_GYRO_CFG, 1, &data))
        return -1;
    st->chip_cfg.gyro_fsr = data >> 3;
    return 0;
}

/**
 *  @brief      Get the accel full-scale range.
 *  @param[out] fsr Current full-scale range.
 *  @return     0 if successful.
 */
int mpu_get_accel_fsr(unsigned char *fsr)
{
    switch (st->chip_cfg.accel_fsr) {
    case INV_FSR_2G:
        fsr[0] = 2;
        break;
    case INV_FSR_4G:
        fsr[0] = 4;
        break;
    case INV_FSR_8G:
        fsr[0] = 8;
        break;
    case INV_FSR_16G:
        fsr[0] = 16;
        break;
    default:
        return -1;
    }
    if (st->chip_cfg.accel_half)
        fsr[0] <<= 1;
    return 0;
}

/**
 *  @brief      Set the accel full-scale range.
 *  @param[in]  fsr Desired full-scale range.
 *  @return     0 if successful.
 */
int mpu_set_accel_fsr(unsigned char fsr)
{
    unsigned char data;

    if (!(st->chip_cfg.sensors))
        return -1;

    switch (fsr) {
    case 2:
        data = INV_FSR_2G << 3;
        break;
    case 4:
        data = INV_FSR_4G << 3;
        break;
    case 8:
        data = INV_FSR_8G << 3;
        break;
    case 16:
        data = INV_FSR_16G << 3;
        break;
    default:
        return -1;
    }

    if (st->chip_cfg.accel_fsr == (data >> 3))
        return 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_CFG, 1, &data))
        return -1;
    st->chip_cfg.accel_fsr = data >> 3;
    return 0;
}

/**
 *  @brief      Get the current DLPF setting.
 *  @param[out] lpf Current LPF setting.
 *  0 if successful.
 */
int mpu_get_lpf(unsigned short *lpf)
{
    switch (st->chip_cfg.lpf) {
    case INV_FILTER_188HZ:
        lpf[0] = 188;
        break;
    case INV_FILTER_98HZ:
        lpf[0] = 98;
        break;
    case INV_FILTER_42HZ:
        lpf[0] = 42;
        break;
    case INV_FILTER_20HZ:
        lpf[0] = 20;
        break;
    case INV_FILTER_10HZ:
        lpf[0] = 10;
        break;
    case INV_FILTER_5HZ:
        lpf[0] = 5;
        break;
    case INV_FILTER_256HZ_NOLPF2:
    case INV_FILTER_2100HZ_NOLPF:
    default:
        lpf[0] = 0;
        break;
    }
    return 0;
}

/**
 *  @brief      Set digital low pass filter.
 *  The following LPF settings are supported: 188, 98, 42, 20, 10, 5.
 *  @param[in]  lpf Desired LPF setting.
 *  @return     0 if successful.
 */
int mpu_set_lpf(unsigned short lpf)
{
    unsigned char data;
    if (!(st->chip_cfg.sensors))
        return -1;

    if (lpf >= 188)
        data = INV_FILTER_188HZ;
    else if (lpf >= 98)
        data = INV_FILTER_98HZ;
    else if (lpf >= 42)
        data = INV_FILTER_42HZ;
    else if (lpf >= 20)
        data = INV_FILTER_20HZ;
    else if (lpf >= 10)
        data = INV_FILTER_10HZ;
    else
        data = INV_FILTER_5HZ;

    if (st->chip_cfg.lpf == data)
        return 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_LPF, 1, &data))
        return -1;
    st->chip_cfg.lpf = data;
    return 0;
}

/**
 *  @brief      Get sampling rate.
 *  @param[out] rate    Current sampling rate (Hz).
 *  @return     0 if successful.
 */
int mpu_get_sample_rate(unsigned short *rate)
{
    if (st->chip_cfg.dmp_on)
        return -1;
    else
        rate[0] = st->chip_cfg.sample_rate;
    return 0;
}

/**
 *  @brief      Set sampling rate.
 *  Sampling rate must be between 4Hz and 1kHz.
 *  @param[in]  rate    Desired sampling rate (Hz).
 *  @return     0 if successful.
 */
int mpu_set_sample_rate(unsigned short rate)
{
    unsigned char data;

    if (!(st->chip_cfg.sensors))
        return -1;

    if (st->chip_cfg.dmp_on)
        return -1;
    else {
        if (st->chip_cfg.lp_accel_mode) {
            if (rate && (rate <= 40)) {
                /* Just stay in low-power accel mode. */
                mpu_lp_accel_mode(rate);
                return 0;
            }
            /* Requested rate exceeds the allowed frequencies in LP accel mode,
             * switch back to full-power mode.
             */
            mpu_lp_accel_mode(0);
        }
        if (rate < 4)
            rate = 4;
        else if (rate > 1000)
            rate = 1000;

        data = 1000 / rate - 1;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_RATE_DIV, 1, &data))
            return -1;

        st->chip_cfg.sample_rate = 1000 / (1 + data);

#ifdef AK89xx_SECONDARY
        mpu_set_compass_sample_rate(inv_min(st->chip_cfg.compass_sample_rate, MAX_COMPASS_SAMPLE_RATE));
#endif

        /* Automatically set LPF to 1/2 sampling rate. */
        mpu_set_lpf(st->chip_cfg.sample_rate >> 1);
        return 0;
    }
}

/**
 *  @brief      Get compass sampling rate.
 *  @param[out] rate    Current compass sampling rate (Hz).
 *  @return     0 if successful.
 */
int mpu_get_compass_sample_rate(unsigned short *rate)
{
#ifdef AK89xx_SECONDARY
    rate[0] = st->chip_cfg.compass_sample_rate;
    return 0;
#else
    rate[0] = 0;
    return -1;
#endif
}

/**
 *  @brief      Set compass sampling rate.
 *  The compass on the auxiliary I2C bus is read by the MPU hardware at a
 *  maximum of 100Hz. The actual rate can be set to a fraction of the gyro
 *  sampling rate.
 *
 *  \n WARNING: The new rate may be different than what was requested. Call
 *  mpu_get_compass_sample_rate to check the actual setting.
 *  @param[in]  rate    Desired compass sampling rate (Hz).
 *  @return     0 if successful.
 */
int mpu_set_compass_sample_rate(unsigned short rate)
{
#ifdef AK89xx_SECONDARY
    unsigned char div;
    if (!rate || rate > st->chip_cfg.sample_rate || rate > MAX_COMPASS_SAMPLE_RATE)
        return -1;

    div = st->chip_cfg.sample_rate / rate - 1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S4_CTRL, 1, &div))
        return -1;
    st->chip_cfg.compass_sample_rate = st->chip_cfg.sample_rate / (div + 1);
    return 0;
#else
    return -1;
#endif
}

/**
 *  @brief      Get gyro sensitivity scale factor.
 *  @param[out] sens    Conversion from hardware units to dps.
 *  @return     0 if successful.
 */
int mpu_get_gyro_sens(float *sens)
{
    switch (st->chip_cfg.gyro_fsr) {
    case INV_FSR_250DPS:
        sens[0] = 131.f;
        break;
    case INV_FSR_500DPS:
        sens[0] = 65.5f;
        break;
    case INV_FSR_1000DPS:
        sens[0] = 32.8f;
        break;
    case INV_FSR_2000DPS:
        sens[0] = 16.4f;
        break;
    default:
        return -1;
    }
    return 0;
}

/**
 *  @brief      Get accel sensitivity scale factor.
 *  @param[out] sens    Conversion from hardware units to g's.
 *  @return     0 if successful.
 */
int mpu_get_accel_sens(unsigned short *sens)
{
    switch (st->chip_cfg.accel_fsr) {
    case INV_FSR_2G:
        sens[0] = 16384;
        break;
    case INV_FSR_4G:
        sens[0] = 8092;
        break;
    case INV_FSR_8G:
        sens[0] = 4096;
        break;
    case INV_FSR_16G:
        sens[0] = 2048;
        break;
    default:
        return -1;
    }
    if (st->chip_cfg.accel_half)
        sens[0] >>= 1;
    return 0;
}

/**
 *  @brief      Get current FIFO configuration.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  @param[out] sensors Mask of sensors in FIFO.
 *  @return     0 if successful.
 */
int mpu_get_fifo_config(unsigned char *sensors)
{
    sensors[0] = st->chip_cfg.fifo_enable;
    return 0;
}

/**
 *  @brief      Select which sensors are pushed to FIFO.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  @param[in]  sensors Mask of sensors to push to FIFO.
 *  @return     0 if successful.
 */
int mpu_configure_fifo(unsigned char sensors)
{
    unsigned char prev;
    int result = 0;

    /* Compass data isn't going into the FIFO. Stop trying. */
    sensors &= ~INV_XYZ_COMPASS;

    if (st->chip_cfg.dmp_on)
        return 0;
    else {
        if (!(st->chip_cfg.sensors))
            return -1;
        prev = st->chip_cfg.fifo_enable;
        st->chip_cfg.fifo_enable = sensors & st->chip_cfg.sensors;
        if (st->chip_cfg.fifo_enable != sensors)
            /* You're not getting what you asked for. Some sensors are
             * asleep.
             */
            result = -1;
        else
            result = 0;
        if (sensors || st->chip_cfg.lp_accel_mode)
            set_int_enable(1);
        else
            set_int_enable(0);
        if (sensors) {
            if (mpu_reset_fifo()) {
                st->chip_cfg.fifo_enable = prev;
                return -1;
            }
        }
    }

    return result;
}

/**
 *  @brief      Get current power state.
 *  @param[in]  power_on    1 if turned on, 0 if suspended.
 *  @return     0 if successful.
 */
int mpu_get_power_state(unsigned char *power_on)
{
    if (st->chip_cfg.sensors)
        power_on[0] = 1;
    else
        power_on[0] = 0;
    return 0;
}

/**
 *  @brief      Turn specific sensors on/off.
 *  @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  \n INV_XYZ_COMPASS
 *  @param[in]  sensors    Mask of sensors to wake.
 *  @return     0 if successful.
 */
int mpu_set_sensors(unsigned char sensors)
{
    unsigned char data;
#ifdef AK89xx_SECONDARY
    unsigned char user_ctrl;
#endif

    if (sensors & INV_XYZ_GYRO)
        data = INV_CLK_PLL;
    else if (sensors)
        data = 0;
    else
        data = BIT_SLEEP;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, &data)) {
        st->chip_cfg.sensors = 0;
        return -1;
    }
    st->chip_cfg.clk_src = data & ~BIT_SLEEP;

    data = 0;
    if (!(sensors & INV_X_GYRO))
        data |= BIT_STBY_XG;
    if (!(sensors & INV_Y_GYRO))
        data |= BIT_STBY_YG;
    if (!(sensors & INV_Z_GYRO))
        data |= BIT_STBY_ZG;
    if (!(sensors & INV_XYZ_ACCEL))
        data |= BIT_STBY_XYZA;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_2, 1, &data)) {
        st->chip_cfg.sensors = 0;
        return -1;
    }

    if (sensors && (sensors != INV_XYZ_ACCEL))
        /* Latched interrupts only used in LP accel mode. */
        mpu_set_int_latched(0);

#ifdef AK89xx_SECONDARY
#ifdef AK89xx_BYPASS
    if (sensors & INV_XYZ_COMPASS)
        mpu_set_bypass(1);
    else
        mpu_set_bypass(0);
#else
    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &user_ctrl))
        return -1;
    /* Handle AKM power management. */
    if (sensors & INV_XYZ_COMPASS) {
        data = AKM_SINGLE_MEASUREMENT;
        user_ctrl |= BIT_AUX_IF_EN;
    } else {
        data = AKM_POWER_DOWN;
        user_ctrl &= ~BIT_AUX_IF_EN;
    }
    if (st->chip_cfg.dmp_on)
        user_ctrl |= BIT_DMP_EN;
    else
        user_ctrl &= ~BIT_DMP_EN;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S1_DO, 1, &data))
        return -1;
    /* Enable/disable I2C master mode. */
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &user_ctrl))
        return -1;
#endif
#endif

    st->chip_cfg.sensors = sensors;
    st->chip_cfg.lp_accel_mode = 0;
    delay_ms(50);
    return 0;
}

/**
 *  @brief      Read the MPU interrupt status registers.
 *  @param[out] status  Mask of interrupt bits.
 *  @return     0 if successful.
 */
int mpu_get_int_status(short *status)
{
    unsigned char tmp[2];
    if (!st->chip_cfg.sensors)
        return -1;
    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_DMP_INT_STATUS, 2, tmp))
        return -1;
    status[0] = (tmp[0] << 8) | tmp[1];
    return 0;
}

/**
 *  @brief      Get one packet from the FIFO.
 *  If @e sensors does not contain a particular sensor, disregard the data
 *  returned to that pointer.
 *  \n @e sensors can contain a combination of the following flags:
 *  \n INV_X_GYRO, INV_Y_GYRO, INV_Z_GYRO
 *  \n INV_XYZ_GYRO
 *  \n INV_XYZ_ACCEL
 *  \n If the FIFO has no new data, @e sensors will be zero.
 *  \n If the FIFO is disabled, @e sensors will be zero and this function will
 *  return a non-zero error code.
 *  @param[out] gyro        Gyro data in hardware units.
 *  @param[out] accel       Accel data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds.
 *  @param[out] sensors     Mask of sensors read from FIFO.
 *  @param[out] more        Number of remaining packets.
 *  @return     0 if successful.
 */
int mpu_read_fifo(short *gyro, short *accel, unsigned long *timestamp,
        unsigned char *sensors, unsigned char *more)
{
    /* Assumes maximum packet size is gyro (6) + accel (6). */
    unsigned char data[MAX_PACKET_LENGTH];
    unsigned char packet_size = 0;
    unsigned short fifo_count, index = 0;

    if (st->chip_cfg.dmp_on)
        return -1;

    sensors[0] = 0;
    if (!st->chip_cfg.sensors)
        return -2;
    if (!st->chip_cfg.fifo_enable)
        return -3;

    if (st->chip_cfg.fifo_enable & INV_X_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_Y_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_Z_GYRO)
        packet_size += 2;
    if (st->chip_cfg.fifo_enable & INV_XYZ_ACCEL)
        packet_size += 6;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_COUNT_H, 2, data))
        return -4;
    if (fifo_count < packet_size)
        return 1;
//    log_i("FIFO count: %hd\n", fifo_count);
    if (fifo_count > (GYRO_HW_MAX_FIFO >> 1)) {
        /* FIFO is 50% full, better check overflow bit. */
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_INT_STATUS, 1, data))
            return -5;
        if (data[0] & BIT_FIFO_OVERFLOW) {
            mpu_reset_fifo();
            return 2;
        }
    }
    if (timestamp)
    	get_ms((unsigned long*)timestamp);

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_R_W, packet_size, data))
        return -7;

    more[0] = fifo_count / packet_size - 1;
    sensors[0] = 0;

    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_XYZ_ACCEL) {
        accel[0] = (data[index+0] << 8) | data[index+1];
        accel[1] = (data[index+2] << 8) | data[index+3];
        accel[2] = (data[index+4] << 8) | data[index+5];
        sensors[0] |= INV_XYZ_ACCEL;
        index += 6;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_X_GYRO) {
        gyro[0] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_X_GYRO;
        index += 2;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_Y_GYRO) {
        gyro[1] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_Y_GYRO;
        index += 2;
    }
    if ((index != packet_size) && st->chip_cfg.fifo_enable & INV_Z_GYRO) {
        gyro[2] = (data[index+0] << 8) | data[index+1];
        sensors[0] |= INV_Z_GYRO;
        index += 2;
    }

    return 0;
}

/**
 *  @brief      Get one unparsed packet from the FIFO.
 *  This function should be used if the packet is to be parsed elsewhere.
 *  @param[in]  length  Length of one FIFO packet.
 *  @param[in]  data    FIFO packet.
 *  @param[in]  more    Number of remaining packets.
 */
int mpu_read_fifo_stream(unsigned short length, unsigned char *data,
    unsigned char *more)
{
    unsigned char tmp[2];
    unsigned short fifo_count;
    if (!st->chip_cfg.dmp_on)
        return -1;
    if (!st->chip_cfg.sensors)
        return -2;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_COUNT_H, 2, tmp))
        return -3;
    fifo_count = (tmp[0] << 8) | tmp[1];
    if (fifo_count < length) {
        more[0] = 0;
        return 1;
    }
    if (fifo_count > (GYRO_HW_MAX_FIFO >> 1)) {
        /* FIFO is 50% full, better check overflow bit. */
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_INT_STATUS, 1, tmp))
            return -5;
        if (tmp[0] & BIT_FIFO_OVERFLOW) {
            mpu_reset_fifo();
            return 2;
        }
    }

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_R_W, length, data))
        return -7;
    more[0] = fifo_count / length - 1;
    return 0;
}

/**
 *  @brief      Set device to bypass mode.
 *  @param[in]  bypass_on   1 to enable bypass mode.
 *  @return     0 if successful.
 */
int mpu_set_bypass(unsigned char bypass_on)
{
    unsigned char tmp;

    if (st->chip_cfg.bypass_mode == bypass_on)
        return 0;

    if (bypass_on) {
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &tmp))
            return -1;
        tmp &= ~BIT_AUX_IF_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &tmp))
            return -1;
        delay_ms(3);
        tmp = BIT_BYPASS_EN;
        if (st->chip_cfg.active_low_int)
            tmp |= BIT_ACTL;
        if (st->chip_cfg.latched_int)
            tmp |= BIT_LATCH_EN | BIT_ANY_RD_CLR;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_PIN_CFG, 1, &tmp))
            return -1;
    } else {
        /* Enable I2C master mode if compass is being used. */
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &tmp))
            return -1;
        if (st->chip_cfg.sensors & INV_XYZ_COMPASS)
            tmp |= BIT_AUX_IF_EN;
        else
            tmp &= ~BIT_AUX_IF_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, &tmp))
            return -1;
        delay_ms(3);
        if (st->chip_cfg.active_low_int)
            tmp = BIT_ACTL;
        else
            tmp = 0;
        if (st->chip_cfg.latched_int)
            tmp |= BIT_LATCH_EN | BIT_ANY_RD_CLR;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_PIN_CFG, 1, &tmp))
            return -1;
    }
    st->chip_cfg.bypass_mode = bypass_on;
    return 0;
}

/**
 *  @brief      Set interrupt level.
 *  @param[in]  active_low  1 for active low, 0 for active high.
 *  @return     0 if successful.
 */
int mpu_set_int_level(unsigned char active_low)
{
    st->chip_cfg.active_low_int = active_low;
    return 0;
}

/**
 *  @brief      Enable latched interrupts.
 *  Any MPU register will clear the interrupt.
 *  @param[in]  enable  1 to enable, 0 to disable.
 *  @return     0 if successful.
 */
int mpu_set_int_latched(unsigned char enable)
{
    unsigned char tmp;
    if (st->chip_cfg.latched_int == enable)
        return 0;

    if (enable)
        tmp = BIT_LATCH_EN | BIT_ANY_RD_CLR;
    else
        tmp = 0;
    if (st->chip_cfg.bypass_mode)
        tmp |= BIT_BYPASS_EN;
    if (st->chip_cfg.active_low_int)
        tmp |= BIT_ACTL;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_PIN_CFG, 1, &tmp))
        return -1;
    st->chip_cfg.latched_int = enable;
    return 0;
}

#ifdef MPU_MAXIMAL

#ifdef MPU6050
static int get_accel_prod_shift(float *st_shift)
{
    unsigned char tmp[4], shift_code[3], ii;

    if (i2c_read(GYRO_HW_ADDR, 0x0D, 4, tmp))
        return 0x07;

    shift_code[0] = ((tmp[0] & 0xE0) >> 3) | ((tmp[3] & 0x30) >> 4);
    shift_code[1] = ((tmp[1] & 0xE0) >> 3) | ((tmp[3] & 0x0C) >> 2);
    shift_code[2] = ((tmp[2] & 0xE0) >> 3) | (tmp[3] & 0x03);
    for (ii = 0; ii < 3; ii++) {
        if (!shift_code[ii]) {
            st_shift[ii] = 0.f;
            continue;
        }
        /* Equivalent to..
         * st_shift[ii] = 0.34f * powf(0.92f/0.34f, (shift_code[ii]-1) / 30.f)
         */
        st_shift[ii] = 0.34f;
        while (--shift_code[ii])
            st_shift[ii] *= 1.034f;
    }
    return 0;
}

static int accel_self_test(long *bias_regular, long *bias_st)
{
    int jj, result = 0;
    float st_shift[3], st_shift_cust, st_shift_var;

    get_accel_prod_shift(st_shift);
    for(jj = 0; jj < 3; jj++) {
        st_shift_cust = labs(bias_regular[jj] - bias_st[jj]) / 65536.f;
        if (st_shift[jj]) {
            st_shift_var = st_shift_cust / st_shift[jj] - 1.f;
            if (fabs(st_shift_var) > test->max_accel_var)
                result |= 1 << jj;
        } else if ((st_shift_cust < test->min_g) ||
            (st_shift_cust > test->max_g))
            result |= 1 << jj;
    }

    return result;
}

static int gyro_self_test(long *bias_regular, long *bias_st)
{
    int jj, result = 0;
    unsigned char tmp[3];
    float st_shift, st_shift_cust, st_shift_var;

    if (i2c_read(GYRO_HW_ADDR, 0x0D, 3, tmp))
        return 0x07;

    tmp[0] &= 0x1F;
    tmp[1] &= 0x1F;
    tmp[2] &= 0x1F;

    for (jj = 0; jj < 3; jj++) {
        st_shift_cust = labs(bias_regular[jj] - bias_st[jj]) / 65536.f;
        if (tmp[jj]) {
            st_shift = 3275.f / test->gyro_sens;
            while (--tmp[jj])
                st_shift *= 1.046f;
            st_shift_var = st_shift_cust / st_shift - 1.f;
            if (fabs(st_shift_var) > test->max_gyro_var)
                result |= 1 << jj;
        } else if ((st_shift_cust < test->min_dps) ||
            (st_shift_cust > test->max_dps))
            result |= 1 << jj;
    }
    return result;
}

#ifdef AK89xx_SECONDARY
static int compass_self_test(void)
{
    unsigned char tmp[6];
    unsigned char tries = 10;
    int result = 0x07;
    short data;

    mpu_set_bypass(1);

    tmp[0] = AKM_POWER_DOWN;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, tmp))
        return 0x07;
    tmp[0] = AKM_BIT_SELF_TEST;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_ASTC, 1, tmp))
        goto AKM_restore;
    tmp[0] = AKM_MODE_SELF_TEST;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, tmp))
        goto AKM_restore;

    do {
        delay_ms(10);
        if (i2c_read(st->chip_cfg.compass_addr, AKM_REG_ST1, 1, tmp))
            goto AKM_restore;
        if (tmp[0] & AKM_DATA_READY)
            break;
    } while (tries--);
    if (!(tmp[0] & AKM_DATA_READY))
        goto AKM_restore;

    if (i2c_read(st->chip_cfg.compass_addr, AKM_REG_HXL, 6, tmp))
        goto AKM_restore;

    result = 0;
    data = (short)(tmp[1] << 8) | tmp[0];
    if ((data > 100) || (data < -100))
        result |= 0x01;
    data = (short)(tmp[3] << 8) | tmp[2];
    if ((data > 100) || (data < -100))
        result |= 0x02;
    data = (short)(tmp[5] << 8) | tmp[4];
    if ((data > -300) || (data < -1000))
        result |= 0x04;

AKM_restore:
    tmp[0] = 0 | SUPPORTS_AK89xx_HIGH_SENS;
    i2c_write(st->chip_cfg.compass_addr, AKM_REG_ASTC, 1, tmp);
    tmp[0] = SUPPORTS_AK89xx_HIGH_SENS;
    i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, tmp);
    mpu_set_bypass(0);
    return result;
}
#endif
#endif

static int get_st_biases(long *gyro, long *accel, unsigned char hw_test)
{
    unsigned char data[MAX_PACKET_LENGTH];
    unsigned char packet_count, ii;
    unsigned short fifo_count;

    data[0] = 0x01;
    data[1] = 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 2, data))
        return -1;
    delay_ms(200);
    data[0] = 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_I2C_MST, 1, data))
        return -1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, data))
        return -1;
    data[0] = BIT_FIFO_RST | BIT_DMP_RST;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, data))
        return -1;
    delay_ms(15);
    data[0] = GYRO_TEST_REG_LPF;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_LPF, 1, data))
        return -1;
    data[0] = GYRO_TEST_REG_RATE_DIV;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_RATE_DIV, 1, data))
        return -1;
    if (hw_test)
        data[0] = GYRO_TEST_REG_GYRO_FSR | 0xE0;
    else
        data[0] = GYRO_TEST_REG_GYRO_FSR;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_GYRO_CFG, 1, data))
        return -1;

    if (hw_test)
        data[0] = GYRO_TEST_REG_ACCEL_FSR | 0xE0;
    else
        data[0] = test->reg_accel_fsr;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_CFG, 1, data))
        return -1;
    if (hw_test)
        delay_ms(200);

    /* Fill FIFO for test->wait_ms milliseconds. */
    data[0] = BIT_FIFO_EN;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 1, data))
        return -1;

    data[0] = INV_XYZ_GYRO | INV_XYZ_ACCEL;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, data))
        return -1;
    delay_ms(test->wait_ms);
    data[0] = 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_FIFO_EN, 1, data))
        return -1;

    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_COUNT_H, 2, data))
        return -1;

    fifo_count = (data[0] << 8) | data[1];
    packet_count = fifo_count / MAX_PACKET_LENGTH;
    gyro[0] = gyro[1] = gyro[2] = 0;
    accel[0] = accel[1] = accel[2] = 0;

    for (ii = 0; ii < packet_count; ii++) {
        short accel_cur[3], gyro_cur[3];
        if (i2c_read(GYRO_HW_ADDR, GYRO_REG_FIFO_R_W, MAX_PACKET_LENGTH, data))
            return -1;
        accel_cur[0] = ((short)data[0] << 8) | data[1];
        accel_cur[1] = ((short)data[2] << 8) | data[3];
        accel_cur[2] = ((short)data[4] << 8) | data[5];
        accel[0] += (long)accel_cur[0];
        accel[1] += (long)accel_cur[1];
        accel[2] += (long)accel_cur[2];
        gyro_cur[0] = (((short)data[6] << 8) | data[7]);
        gyro_cur[1] = (((short)data[8] << 8) | data[9]);
        gyro_cur[2] = (((short)data[10] << 8) | data[11]);
        gyro[0] += (long)gyro_cur[0];
        gyro[1] += (long)gyro_cur[1];
        gyro[2] += (long)gyro_cur[2];
    }
#ifdef EMPL_NO_64BIT
    gyro[0] = (long)(((float)gyro[0]*65536.f) / test->gyro_sens / packet_count);
    gyro[1] = (long)(((float)gyro[1]*65536.f) / test->gyro_sens / packet_count);
    gyro[2] = (long)(((float)gyro[2]*65536.f) / test->gyro_sens / packet_count);
    if (has_accel) {
        accel[0] = (long)(((float)accel[0]*65536.f) / test->accel_sens /
            packet_count);
        accel[1] = (long)(((float)accel[1]*65536.f) / test->accel_sens /
            packet_count);
        accel[2] = (long)(((float)accel[2]*65536.f) / test->accel_sens /
            packet_count);
        /* Don't remove gravity! */
        accel[2] -= 65536L;
    }
#else
    gyro[0] = (long)(((long long)gyro[0]<<16) / test->gyro_sens / packet_count);
    gyro[1] = (long)(((long long)gyro[1]<<16) / test->gyro_sens / packet_count);
    gyro[2] = (long)(((long long)gyro[2]<<16) / test->gyro_sens / packet_count);
    accel[0] = (long)(((long long)accel[0]<<16) / test->accel_sens /
        packet_count);
    accel[1] = (long)(((long long)accel[1]<<16) / test->accel_sens /
        packet_count);
    accel[2] = (long)(((long long)accel[2]<<16) / test->accel_sens /
        packet_count);
    /* Don't remove gravity! */
    if (accel[2] > 0L)
        accel[2] -= 65536L;
    else
        accel[2] += 65536L;
#endif

    return 0;
}

/**
 *  @brief      Trigger gyro/accel/compass self-test->
 *  On success/error, the self-test returns a mask representing the sensor(s)
 *  that failed. For each bit, a one (1) represents a "pass" case; conversely,
 *  a zero (0) indicates a failure.
 *
 *  \n The mask is defined as follows:
 *  \n Bit 0:   Gyro.
 *  \n Bit 1:   Accel.
 *  \n Bit 2:   Compass.
 *
 *  \n Currently, the hardware self-test is unsupported for MPU6500. However,
 *  this function can still be used to obtain the accel and gyro biases.
 *
 *  \n This function must be called with the device either face-up or face-down
 *  (z-axis is parallel to gravity).
 *  @param[out] gyro        Gyro biases in q16 format.
 *  @param[out] accel       Accel biases (if applicable) in q16 format.
 *  @return     Result mask (see above).
 */
int mpu_run_self_test(long *gyro, long *accel)
{
#ifdef MPU6050
    const unsigned char tries = 2;
    long gyro_st[3], accel_st[3];
    unsigned char accel_result, gyro_result;
#ifdef AK89xx_SECONDARY
    unsigned char compass_result;
#endif
    int ii;
#endif
    int result;
    unsigned char accel_fsr, fifo_sensors, sensors_on;
    unsigned short gyro_fsr, sample_rate, lpf;
    unsigned char dmp_was_on;

    if (st->chip_cfg.dmp_on) {
        mpu_set_dmp_state(0);
        dmp_was_on = 1;
    } else
        dmp_was_on = 0;

    /* Get initial settings. */
    mpu_get_gyro_fsr(&gyro_fsr);
    mpu_get_accel_fsr(&accel_fsr);
    mpu_get_lpf(&lpf);
    mpu_get_sample_rate(&sample_rate);
    sensors_on = st->chip_cfg.sensors;
    mpu_get_fifo_config(&fifo_sensors);

    /* For older chips, the self-test will be different. */
#if defined MPU6050
    for (ii = 0; ii < tries; ii++)
        if (!get_st_biases(gyro, accel, 0))
            break;
    if (ii == tries) {
        /* If we reach this point, we most likely encountered an I2C error.
         * We'll just report an error for all three sensors.
         */
        result = 0;
        goto restore;
    }
    for (ii = 0; ii < tries; ii++)
        if (!get_st_biases(gyro_st, accel_st, 1))
            break;
    if (ii == tries) {
        /* Again, probably an I2C error. */
        result = 0;
        goto restore;
    }
    accel_result = accel_self_test(accel, accel_st);
    gyro_result = gyro_self_test(gyro, gyro_st);

    result = 0;
    if (!gyro_result)
        result |= 0x01;
    if (!accel_result)
        result |= 0x02;

#ifdef AK89xx_SECONDARY
    compass_result = compass_self_test();
    if (!compass_result)
        result |= 0x04;
#endif
restore:
#elif defined MPU6500
    /* For now, this function will return a "pass" result for all three sensors
     * for compatibility with current test applications.
     */
    get_st_biases(gyro, accel, 0);
    result = 0x7;
#endif
    /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.clk_src = INV_CLK_PLL;
    mpu_set_gyro_fsr(gyro_fsr);
    mpu_set_accel_fsr(accel_fsr);
    mpu_set_lpf(lpf);
    mpu_set_sample_rate(sample_rate);
    mpu_set_sensors(sensors_on);
    mpu_configure_fifo(fifo_sensors);

    if (dmp_was_on)
        mpu_set_dmp_state(1);

    return result;
}
#endif // MPU_MAXIMAL

/**
 *  @brief      Write to the DMP memory.
 *  This function prevents I2C writes past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to write.
 *  @param[in]  data        Bytes to write to memory.
 *  @return     0 if successful.
 */
int mpu_write_mem(unsigned short mem_addr, unsigned short length,
        unsigned char *data)
{
    unsigned char tmp[2];

    if (!data)
        return -1;
    if (!st->chip_cfg.sensors)
        return -2;

    tmp[0] = (unsigned char)(mem_addr >> 8);
    tmp[1] = (unsigned char)(mem_addr & 0xFF);

    /* Check bank boundaries. */
    if (tmp[1] + length > GYRO_HW_BANK_SIZE)
        return -3;

    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_BANK_SEL, 2, tmp))
        return -4;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_MEM_R_W, length, data))
        return -5;
    return 0;
}

/**
 *  @brief      Read from the DMP memory.
 *  This function prevents I2C reads past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to read.
 *  @param[out] data        Bytes read from memory.
 *  @return     0 if successful.
 */
int mpu_read_mem(unsigned short mem_addr, unsigned short length,
        unsigned char *data)
{
    unsigned char tmp[2];

    if (!data)
        return -1;
    if (!st->chip_cfg.sensors)
        return -1;

    tmp[0] = (unsigned char)(mem_addr >> 8);
    tmp[1] = (unsigned char)(mem_addr & 0xFF);

    /* Check bank boundaries. */
    if (tmp[1] + length > GYRO_HW_BANK_SIZE)
        return -1;

    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_BANK_SEL, 2, tmp))
        return -1;
    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_MEM_R_W, length, data))
        return -1;
    return 0;
}

/**
 *  @brief      Load and verify DMP image.
 *  @param[in]  length      Length of DMP image.
 *  @param[in]  firmware    DMP code.
 *  @param[in]  start_addr  Starting address of DMP code memory.
 *  @param[in]  sample_rate Fixed sampling rate used when DMP is enabled.
 *  @return     0 if successful.
 */

//!!! modified to use compressed DMP code
int mpu_load_firmware(unsigned short start_addr, unsigned short sample_rate)
{
    unsigned short ii;
    unsigned short this_write;
    int errCode;

    /* Must divide evenly into GYRO_HW_BANK_SIZE to avoid bank crossings. */
#define LOAD_CHUNK  (16)
    uint8_t progBuffer[LOAD_CHUNK];
    unsigned char cur[LOAD_CHUNK], tmp[2];

    if (st->chip_cfg.dmp_loaded)
        /* DMP should only be loaded once. */
      return -1;

    /* start loading */
    inv_dmp_uncompress_reset();
    
    for (ii = 0; ii < UNCOMPRESSED_DMP_CODE_SIZE; ii += this_write) {
        this_write = inv_min(LOAD_CHUNK, UNCOMPRESSED_DMP_CODE_SIZE - ii);
        
        for (unsigned progIndex = 0; progIndex < this_write; progIndex++)
	    progBuffer[progIndex] = inv_dmp_uncompress();
            
        if ((errCode = mpu_write_mem(ii, this_write, progBuffer))) {
#ifdef MPU_DEBUG
            Serial.print("fimrware write failed: ");
            Serial.println(errCode);
#endif
            return -3;
        }
        
        if (mpu_read_mem(ii, this_write, cur))
            return -4;
            
        if (memcmp(progBuffer, cur, this_write)) {
#ifdef MPU_DEBUG
            Serial.print("Firmware compare failed addr "); Serial.println(ii);
            for (int i = 0; i < 10; i++) {
              Serial.print(progBuffer[i]); 
              Serial.print(" ");
            }
            Serial.println();
            for (int i = 0; i < 10; i++) {
              Serial.print(cur[i]); 
              Serial.print(" ");
            }
            Serial.println();
#endif
            return -5;
        }
    }

    /* Set program start address. */
    tmp[0] = start_addr >> 8;
    tmp[1] = start_addr & 0xFF;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PRGM_START_H, 2, tmp))
        return -6;

    st->chip_cfg.dmp_loaded = 1;
    st->chip_cfg.dmp_sample_rate = sample_rate;
#ifdef MPU_DEBUG
    Serial.println("Firmware loaded");
#endif
    return 0;
}

/**
 *  @brief      Enable/disable DMP support.
 *  @param[in]  enable  1 to turn on the DMP.
 *  @return     0 if successful.
 */
int mpu_set_dmp_state(unsigned char enable)
{
    unsigned char tmp;
    if (st->chip_cfg.dmp_on == enable)
        return 0;

    if (enable) {
        if (!st->chip_cfg.dmp_loaded)
            return -1;
        /* Disable data ready interrupt. */
        set_int_enable(0);
        /* Disable bypass mode. */
        mpu_set_bypass(0);
        /* Keep constant sample rate, FIFO rate controlled by DMP. */
        mpu_set_sample_rate(st->chip_cfg.dmp_sample_rate);
        /* Remove FIFO elements. */
        tmp = 0;
        i2c_write(GYRO_HW_ADDR, 0x23, 1, &tmp);
        st->chip_cfg.dmp_on = 1;
        /* Enable DMP interrupt. */
        set_int_enable(1);
        mpu_reset_fifo();
    } else {
        /* Disable DMP interrupt. */
        set_int_enable(0);
        /* Restore FIFO settings. */
        tmp = st->chip_cfg.fifo_enable;
        i2c_write(GYRO_HW_ADDR, 0x23, 1, &tmp);
        st->chip_cfg.dmp_on = 0;
        mpu_reset_fifo();
    }
    return 0;
}

/**
 *  @brief      Get DMP state.
 *  @param[out] enabled 1 if enabled.
 *  @return     0 if successful.
 */
int mpu_get_dmp_state(unsigned char *enabled)
{
    enabled[0] = st->chip_cfg.dmp_on;
    return 0;
}


/* This initialization is similar to the one in ak8975.c. */
static int setup_compass(void)
{
#ifdef AK89xx_SECONDARY
    unsigned char data[4], akm_addr;

    mpu_set_bypass(1);

    /* Find compass. Possible addresses range from 0x0C to 0x0F. */
    for (akm_addr = 0x0C; akm_addr <= 0x0F; akm_addr++) {
        int result;
        result = i2c_read(akm_addr, AKM_REG_WHOAMI, 1, data);
        if (!result && (data[0] == AKM_WHOAMI))
            break;
    }

    if (akm_addr > 0x0F) {
        /* TODO: Handle this case in all compass-related functions. */
#ifdef MPU_DEBUG
        Serial.println("Compass not found.");
#endif
        return -1;
    }

    st->chip_cfg.compass_addr = akm_addr;

    data[0] = AKM_POWER_DOWN;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, data))
        return -2;
    delay_ms(1);

    data[0] = AKM_FUSE_ROM_ACCESS;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, data))
        return -3;
    delay_ms(1);

    /* Get sensitivity adjustment data from fuse ROM. */
    if (i2c_read(st->chip_cfg.compass_addr, AKM_REG_ASAX, 3, data))
        return -4;
    st->chip_cfg.mag_sens_adj[0] = (long)data[0] + 128;
    st->chip_cfg.mag_sens_adj[1] = (long)data[1] + 128;
    st->chip_cfg.mag_sens_adj[2] = (long)data[2] + 128;
#ifdef MPU_DEBUG
    Serial.print("Compass sens: "); Serial.print(st->chip_cfg.mag_sens_adj[0]); Serial.print(" ");
    Serial.print(st->chip_cfg.mag_sens_adj[1]); Serial.print(" ");
    Serial.print(st->chip_cfg.mag_sens_adj[2]); Serial.println();
#endif

    data[0] = AKM_POWER_DOWN;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, data))
        return -5;
    delay_ms(1);

    mpu_set_bypass(0);

    /* Set up master mode, master clock, and ES bit. */
    data[0] = 0x40;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_I2C_MST, 1, data))
        return -6;

    /* Slave 0 reads from AKM data registers. */
    data[0] = BIT_I2C_READ | st->chip_cfg.compass_addr;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S0_ADDR, 1, data))
        return -7;

    /* Compass reads start at this register. */
    data[0] = AKM_REG_ST1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S0_REG, 1, data))
        return -8;

    /* Enable slave 0, 8-byte reads. */
    data[0] = BIT_SLAVE_EN | 8;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S0_CTRL, 1, data))
        return -9;

    /* Slave 1 changes AKM measurement mode. */
    data[0] = st->chip_cfg.compass_addr;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S1_ADDR, 1, data))
        return -10;

    /* AKM measurement mode register. */
    data[0] = AKM_REG_CNTL;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S1_REG, 1, data))
        return -11;

    /* Enable slave 1, 1-byte writes. */
    data[0] = BIT_SLAVE_EN | 1;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S1_CTRL, 1, data))
        return -12;

    /* Set slave 1 data. */
    data[0] = AKM_SINGLE_MEASUREMENT;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_S1_DO, 1, data))
        return -13;

    /* Trigger slave 0 and slave 1 actions at each sample. */
    data[0] = 0x03;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_I2C_DELAY_CTRL, 1, data))
        return -14;

#ifdef MPU9150
    /* For the MPU9150, the auxiliary I2C bus needs to be set to VDD. */
    data[0] = BIT_I2C_MST_VDDIO;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_YG_OFFS_TC, 1, data))
        return -15;
#endif

    return 0;
#else
    return -16;
#endif
}

/**
 *  @brief      Read raw compass data.
 *  @param[out] data        Raw data in hardware units.
 *  @param[out] timestamp   Timestamp in milliseconds. Null if not needed.
 *  @return     0 if successful.
 */
int mpu_get_compass_reg(short *data, unsigned long *timestamp)
{
#ifdef AK89xx_SECONDARY
    unsigned char tmp[9];

    if (!(st->chip_cfg.sensors & INV_XYZ_COMPASS))
        return -1;

#ifdef AK89xx_BYPASS
    if (i2c_read(st->chip_cfg.compass_addr, AKM_REG_ST1, 8, tmp))
        return -2;
    tmp[8] = AKM_SINGLE_MEASUREMENT;
    if (i2c_write(st->chip_cfg.compass_addr, AKM_REG_CNTL, 1, tmp+8))
        return -3;
#else
    if (i2c_read(GYRO_HW_ADDR, GYRO_REG_RAW_COMPASS, 8, tmp))
        return -4;
#endif

#if defined AK8975_SECONDARY
    /* AK8975 doesn't have the overrun error bit. */
    if (!(tmp[0] & AKM_DATA_READY))
        return -5;
    if ((tmp[7] & AKM_OVERFLOW) || (tmp[7] & AKM_DATA_ERROR))
        return -6;
#elif defined AK8963_SECONDARY
    /* AK8963 doesn't have the data read error bit. */
    if (!(tmp[0] & AKM_DATA_READY) || (tmp[0] & AKM_DATA_OVERRUN))
        return -7;
    if (tmp[7] & AKM_OVERFLOW)
        return -8;
#endif
    data[0] = ((unsigned short)tmp[2] << 8) | (unsigned short)tmp[1];
    data[1] = ((unsigned short)tmp[4] << 8) | (unsigned short)tmp[3];
    data[2] = ((unsigned short)tmp[6] << 8) | (unsigned short)tmp[5];

    data[0] = ((long)data[0] * st->chip_cfg.mag_sens_adj[0]) >> 8;
    data[1] = ((long)data[1] * st->chip_cfg.mag_sens_adj[1]) >> 8;
    data[2] = ((long)data[2] * st->chip_cfg.mag_sens_adj[2]) >> 8;

    if (timestamp)
        get_ms(timestamp);
    return 0;
#else
    return -9;
#endif
}

/**
 *  @brief      Get the compass full-scale range.
 *  @param[out] fsr Current full-scale range.
 *  @return     0 if successful.
 */
int mpu_get_compass_fsr(unsigned short *fsr)
{
#ifdef AK89xx_SECONDARY
    fsr[0] = GYRO_HW_COMPASS_FSR;
    return 0;
#else
    return -1;
#endif
}

/**
 *  @brief      Enters LP accel motion interrupt mode.
 *  The behavior of this feature is very different between the MPU6050 and the
 *  MPU6500. Each chip's version of this feature is explained below.
 *
 *  \n MPU6050:
 *  \n When this mode is first enabled, the hardware captures a single accel
 *  sample, and subsequent samples are compared with this one to determine if
 *  the device is in motion. Therefore, whenever this "locked" sample needs to
 *  be changed, this function must be called again.
 *
 *  \n The hardware motion threshold can be between 32mg and 8160mg in 32mg
 *  increments.
 *
 *  \n Low-power accel mode supports the following frequencies:
 *  \n 1.25Hz, 5Hz, 20Hz, 40Hz
 *
 *  \n MPU6500:
 *  \n Unlike the MPU6050 version, the hardware does not "lock in" a reference
 *  sample. The hardware monitors the accel data and detects any large change
 *  over a short period of time.
 *
 *  \n The hardware motion threshold can be between 4mg and 1020mg in 4mg
 *  increments.
 *
 *  \n MPU6500 Low-power accel mode supports the following frequencies:
 *  \n 1.25Hz, 2.5Hz, 5Hz, 10Hz, 20Hz, 40Hz, 80Hz, 160Hz, 320Hz, 640Hz
 *
 *  \n\n NOTES:
 *  \n The driver will round down @e thresh to the nearest supported value if
 *  an unsupported threshold is selected.
 *  \n To select a fractional wake-up frequency, round down the value passed to
 *  @e lpa_freq.
 *  \n The MPU6500 does not support a delay parameter. If this function is used
 *  for the MPU6500, the value passed to @e time will be ignored.
 *  \n To disable this mode, set @e lpa_freq to zero. The driver will restore
 *  the previous configuration.
 *
 *  @param[in]  thresh      Motion threshold in mg.
 *  @param[in]  time        Duration in milliseconds that the accel data must
 *                          exceed @e thresh before motion is reported.
 *  @param[in]  lpa_freq    Minimum sampling rate, or zero to disable.
 *  @return     0 if successful.
 */
int mpu_lp_motion_interrupt(unsigned short thresh, unsigned char time,
    unsigned char lpa_freq)
{
    unsigned char data[3];

    if (lpa_freq) {
        unsigned char thresh_hw;

#if defined MPU6050
        /* TODO: Make these const/#defines. */
        /* 1LSb = 32mg. */
        if (thresh > 8160)
            thresh_hw = 255;
        else if (thresh < 32)
            thresh_hw = 1;
        else
            thresh_hw = thresh >> 5;
#elif defined MPU6500
        /* 1LSb = 4mg. */
        if (thresh > 1020)
            thresh_hw = 255;
        else if (thresh < 4)
            thresh_hw = 1;
        else
            thresh_hw = thresh >> 2;
#endif

        if (!time)
            /* Minimum duration must be 1ms. */
            time = 1;

#if defined MPU6050
        if (lpa_freq > 40)
#elif defined MPU6500
        if (lpa_freq > 640)
#endif
            /* At this point, the chip has not been re-configured, so the
             * function can safely exit.
             */
            return -1;

        if (!st->chip_cfg.int_motion_only) {
            /* Store current settings for later. */
            if (st->chip_cfg.dmp_on) {
                mpu_set_dmp_state(0);
                st->chip_cfg.cache.dmp_on = 1;
            } else
                st->chip_cfg.cache.dmp_on = 0;
            mpu_get_gyro_fsr(&st->chip_cfg.cache.gyro_fsr);
            mpu_get_accel_fsr(&st->chip_cfg.cache.accel_fsr);
            mpu_get_lpf(&st->chip_cfg.cache.lpf);
            mpu_get_sample_rate(&st->chip_cfg.cache.sample_rate);
            st->chip_cfg.cache.sensors_on = st->chip_cfg.sensors;
            mpu_get_fifo_config(&st->chip_cfg.cache.fifo_sensors);
        }

#ifdef MPU6050
        /* Disable hardware interrupts for now. */
        set_int_enable(0);

        /* Enter full-power accel-only mode. */
        mpu_lp_accel_mode(0);

        /* Override current LPF (and HPF) settings to obtain a valid accel
         * reading.
         */
        data[0] = INV_FILTER_256HZ_NOLPF2;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_LPF, 1, data))
            return -1;

        /* NOTE: Digital high pass filter should be configured here. Since this
         * driver doesn't modify those bits anywhere, they should already be
         * cleared by default.
         */

        /* Configure the device to send motion interrupts. */
        /* Enable motion interrupt. */
        data[0] = BIT_MOT_INT_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, data))
            goto lp_int_restore;

        /* Set motion interrupt parameters. */
        data[0] = thresh_hw;
        data[1] = time;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_MOTION_THR, 2, data))
            goto lp_int_restore;

        /* Force hardware to "lock" current accel sample. */
        delay_ms(5);
        data[0] = (st->chip_cfg.accel_fsr << 3) | BITS_HPF;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_CFG, 1, data))
            goto lp_int_restore;

        /* Set up LP accel mode. */
        data[0] = BIT_LPA_CYCLE;
        if (lpa_freq == 1)
            data[1] = INV_LPA_1_25HZ;
        else if (lpa_freq <= 5)
            data[1] = INV_LPA_5HZ;
        else if (lpa_freq <= 20)
            data[1] = INV_LPA_20HZ;
        else
            data[1] = INV_LPA_40HZ;
        data[1] = (data[1] << 6) | BIT_STBY_XYZG;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 2, data))
            goto lp_int_restore;

        st->chip_cfg.int_motion_only = 1;
        return 0;
#elif defined MPU6500
        /* Disable hardware interrupts. */
        set_int_enable(0);

        /* Enter full-power accel-only mode, no FIFO/DMP. */
        data[0] = 0;
        data[1] = 0;
        data[2] = BIT_STBY_XYZG;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_USER_CTRL, 3, data))
            goto lp_int_restore;

        /* Set motion threshold. */
        data[0] = thresh_hw;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_MOTION_THR, 1, data))
            goto lp_int_restore;

        /* Set wake frequency. */
        if (lpa_freq == 1)
            data[0] = INV_LPA_1_25HZ;
        else if (lpa_freq == 2)
            data[0] = INV_LPA_2_5HZ;
        else if (lpa_freq <= 5)
            data[0] = INV_LPA_5HZ;
        else if (lpa_freq <= 10)
            data[0] = INV_LPA_10HZ;
        else if (lpa_freq <= 20)
            data[0] = INV_LPA_20HZ;
        else if (lpa_freq <= 40)
            data[0] = INV_LPA_40HZ;
        else if (lpa_freq <= 80)
            data[0] = INV_LPA_80HZ;
        else if (lpa_freq <= 160)
            data[0] = INV_LPA_160HZ;
        else if (lpa_freq <= 320)
            data[0] = INV_LPA_320HZ;
        else
            data[0] = INV_LPA_640HZ;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_LP_ACCEL_ODR, 1, data))
            goto lp_int_restore;

        /* Enable motion interrupt (MPU6500 version). */
        data[0] = BITS_WOM_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_INTEL, 1, data))
            goto lp_int_restore;

        /* Enable cycle mode. */
        data[0] = BIT_LPA_CYCLE;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_PWR_MGMT_1, 1, data))
            goto lp_int_restore;

        /* Enable interrupt. */
        data[0] = BIT_MOT_INT_EN;
        if (i2c_write(GYRO_HW_ADDR, GYRO_REG_INT_ENABLE, 1, data))
            goto lp_int_restore;

        st->chip_cfg.int_motion_only = 1;
        return 0;
#endif
    } else {
        /* Don't "restore" the previous state if no state has been saved. */
        unsigned ii;
        char *cache_ptr = (char*)&st->chip_cfg.cache;
        for (ii = 0; ii < sizeof(st->chip_cfg.cache); ii++) {
            if (cache_ptr[ii] != 0)
                goto lp_int_restore;
        }
        /* If we reach this point, motion interrupt mode hasn't been used yet. */
        return -1;
    }
lp_int_restore:
    /* Set to invalid values to ensure no I2C writes are skipped. */
    st->chip_cfg.gyro_fsr = 0xFF;
    st->chip_cfg.accel_fsr = 0xFF;
    st->chip_cfg.lpf = 0xFF;
    st->chip_cfg.sample_rate = 0xFFFF;
    st->chip_cfg.sensors = 0xFF;
    st->chip_cfg.fifo_enable = 0xFF;
    st->chip_cfg.clk_src = INV_CLK_PLL;
    mpu_set_sensors(st->chip_cfg.cache.sensors_on);
    mpu_set_gyro_fsr(st->chip_cfg.cache.gyro_fsr);
    mpu_set_accel_fsr(st->chip_cfg.cache.accel_fsr);
    mpu_set_lpf(st->chip_cfg.cache.lpf);
    mpu_set_sample_rate(st->chip_cfg.cache.sample_rate);
    mpu_configure_fifo(st->chip_cfg.cache.fifo_sensors);

    if (st->chip_cfg.cache.dmp_on)
        mpu_set_dmp_state(1);

#ifdef MPU6500
    /* Disable motion interrupt (MPU6500 version). */
    data[0] = 0;
    if (i2c_write(GYRO_HW_ADDR, GYRO_REG_ACCEL_INTEL, 1, data))
        goto lp_int_restore;
#endif

    st->chip_cfg.int_motion_only = 0;
    return 0;
}

/**
 *  @}
 */

