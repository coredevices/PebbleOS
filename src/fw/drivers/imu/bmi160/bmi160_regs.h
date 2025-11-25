/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

// BMI160 Register Map
static const uint8_t BMI160_REG_CHIP_ID = 0x00;
static const uint8_t BMI160_REG_ERR = 0x02;
static const uint8_t BMI160_REG_PMU_STATUS = 0x03;

// Current sensor data
static const uint8_t BMI160_REG_DATA_0 = 0x04;
// Register names differ from those in datasheet
static const uint8_t BMI160_REG_MAG_X_LSB = 0x04;
static const uint8_t BMI160_REG_MAG_X_MSB = 0x05;
static const uint8_t BMI160_REG_MAG_Y_LSB = 0x06;
static const uint8_t BMI160_REG_MAG_Y_MSB = 0x07;
static const uint8_t BMI160_REG_MAG_Z_LSB = 0x08;
static const uint8_t BMI160_REG_MAG_Z_MSB = 0x09;
static const uint8_t BMI160_REG_RHALL_LSB = 0x0A;
static const uint8_t BMI160_REG_RHALL_MSB = 0x0B;
static const uint8_t BMI160_REG_GYR_X_LSB = 0x0C;
static const uint8_t BMI160_REG_GYR_X_MSB = 0x0D;
static const uint8_t BMI160_REG_GYR_Y_LSB = 0x0E;
static const uint8_t BMI160_REG_GYR_Y_MSB = 0x0F;
static const uint8_t BMI160_REG_GYR_Z_LSB = 0x10;
static const uint8_t BMI160_REG_GYR_Z_MSB = 0x11;
static const uint8_t BMI160_REG_ACC_X_LSB = 0x12;
static const uint8_t BMI160_REG_ACC_X_MSB = 0x13;
static const uint8_t BMI160_REG_ACC_Y_LSB = 0x14;
static const uint8_t BMI160_REG_ACC_Y_MSB = 0x15;
static const uint8_t BMI160_REG_ACC_Z_LSB = 0x16;
static const uint8_t BMI160_REG_ACC_Z_MSB = 0x17;
// Sensor time is stored in 24-bit little-endian format (LSB in BMI160_REG_SENSORTIME_0)
static const uint8_t BMI160_REG_SENSORTIME_0 = 0x18;
static const uint8_t BMI160_REG_SENSORTIME_1 = 0x19;
static const uint8_t BMI160_REG_SENSORTIME_2 = 0x1A;

static const uint8_t BMI160_SENSORTIME_RESOLUTION_US = 39;

// Status registers
static const uint8_t BMI160_REG_STATUS = 0x1B;
static const uint8_t BMI160_REG_INT_STATUS_0 = 0x1C;
static const uint8_t BMI160_REG_INT_STATUS_1 = 0x1D;
static const uint8_t BMI160_REG_INT_STATUS_2 = 0x1E;
static const uint8_t BMI160_REG_INT_STATUS_3 = 0x1F;
static const uint8_t BMI160_REG_TEMPERATURE_LSB = 0x20;
static const uint8_t BMI160_REG_TEMPERATURE_MSB = 0x21;

// FIFO
static const uint8_t BMI160_REG_FIFO_LENGTH_LSB = 0x22;
static const uint8_t BMI160_REG_FIFO_LENGTH_MSB = 0x23;
static const uint8_t BMI160_REG_FIFO_DOWNS = 0x45;
static const uint8_t BMI160_REG_FIFO_CONFIG_0 = 0x46;
static const uint8_t BMI160_REG_FIFO_CONFIG_1 = 0x47;
static const uint8_t BMI160_REG_FIFO_DATA = 0x24;

static const uint8_t BMI160_REG_ACC_CONF = 0x40;
static const uint8_t BMI160_REG_ACC_RANGE = 0x41;
static const uint8_t BMI160_REG_GYR_CONF = 0x42;
static const uint8_t BMI160_REG_GYR_RANGE = 0x43;
static const uint8_t BMI160_REG_MAG_CONF = 0x44;

// Magnetometer interface configuration
static const uint8_t BMI160_REG_MAG_I2C_DEVICE_ADDR = 0x4B;
static const uint8_t BMI160_REG_MAG_IF_1 = 0x4C;
static const uint8_t BMI160_MAG_IF_1_MANUAL_MODE_ENABLE = 0x80;

static const uint8_t BMI160_REG_MAG_READ_ADDR = 0x4D;
static const uint8_t BMI160_REG_MAG_WRITE_ADDR = 0x4E;
static const uint8_t BMI160_REG_MAG_WRITE_DATA = 0x4F;

// Interrupt configuration
static const uint8_t BMI160_REG_INT_EN_0 = 0x50;
static const uint8_t BMI160_REG_INT_EN_1 = 0x51;
static const uint8_t BMI160_REG_INT_EN_2 = 0x52;
static const uint8_t BMI160_REG_INT_OUT_CTRL = 0x53;
static const uint8_t BMI160_REG_INT_LATCH = 0x54;
static const uint8_t BMI160_REG_INT_MAP_0 = 0x55;
static const uint8_t BMI160_REG_INT_MAP_1 = 0x56;
static const uint8_t BMI160_REG_INT_MAP_2 = 0x57;
static const uint8_t BMI160_REG_INT_DATA_0 = 0x58;
static const uint8_t BMI160_REG_INT_DATA_1 = 0x59;
static const uint8_t BMI160_REG_INT_LOWHIGH_0 = 0x5A;
static const uint8_t BMI160_REG_INT_LOWHIGH_1 = 0x5B;
static const uint8_t BMI160_REG_INT_LOWHIGH_2 = 0x5C;
static const uint8_t BMI160_REG_INT_LOWHIGH_3 = 0x5D;
static const uint8_t BMI160_REG_INT_LOWHIGH_4 = 0x5E;
static const uint8_t BMI160_REG_INT_MOTION_0 = 0x5F;
static const uint8_t BMI160_REG_INT_MOTION_1 = 0x60;
static const uint8_t BMI160_REG_INT_MOTION_2 = 0x61;
static const uint8_t BMI160_REG_INT_MOTION_3 = 0x62;
static const uint8_t BMI160_REG_INT_TAP_0 = 0x63;
static const uint8_t BMI160_REG_INT_TAP_1 = 0x64;
static const uint8_t BMI160_REG_INT_ORIENT_0 = 0x65;
static const uint8_t BMI160_REG_INT_ORIENT_1 = 0x66;
static const uint8_t BMI160_REG_INT_FLAT_0 = 0x67;
static const uint8_t BMI160_REG_INT_FLAT_1 = 0x68;

static const uint8_t BMI160_REG_FOC_CONF = 0x69;
static const uint8_t BMI160_REG_CONF = 0x6A;

static const uint8_t BMI160_REG_IF_CONF = 0x6B;
static const uint8_t BMI160_IF_CONF_MAG_ENABLE = 0x20;  // Undocumented

static const uint8_t BMI160_REG_PMU_TRIGGER = 0x6C;
static const uint8_t BMI160_REG_SELF_TEST = 0x6D;
static const uint8_t BMI160_REG_NV_CONF = 0x70;

static const uint8_t BMI160_REG_OFFSET_ACC_X = 0x71;
static const uint8_t BMI160_REG_OFFSET_ACC_Y = 0x72;
static const uint8_t BMI160_REG_OFFSET_ACC_Z = 0x73;
static const uint8_t BMI160_REG_OFFSET_GYR_X_LSB = 0x74;
static const uint8_t BMI160_REG_OFFSET_GYR_Y_LSB = 0x75;
static const uint8_t BMI160_REG_OFFSET_GYR_Z_LSB = 0x76;
static const uint8_t BMI160_REG_OFFSET_7 = 0x77;

static const uint8_t BMI160_REG_STEPCOUNTER_LSB = 0x78;
static const uint8_t BMI160_REG_STEPCOUNTER_MSB = 0x79;
static const uint8_t BMI160_REG_INT_STEP_DET_CONF_0 = 0x7A;
static const uint8_t BMI160_REG_STEPCOUNTER_CONF = 0x7B;

static const uint8_t BMI160_REG_CMD = 0x7E;
static const uint8_t BMI160_CMD_START_FOC = 0x03;
// To set the PMU mode, bitwise-or the command with the desired mode
// from the BMI160*PowerMode enums.
static const uint8_t BMI160_CMD_ACC_SET_PMU_MODE = 0x10;
static const uint8_t BMI160_CMD_GYR_SET_PMU_MODE = 0x14;
static const uint8_t BMI160_CMD_MAG_SET_PMU_MODE = 0x18;

static const uint8_t BMI160_CMD_PROG_NVM = 0xA0;
static const uint8_t BMI160_CMD_FIFO_FLUSH = 0xB0;
static const uint8_t BMI160_CMD_INT_RESET = 0xB1;
static const uint8_t BMI160_CMD_SOFTRESET = 0xB6;
static const uint8_t BMI160_CMD_STEP_CNT_CLR = 0xB2;

// Command sequence to enable "extended mode"
static const uint8_t BMI160_CMD_EXTMODE_EN_FIRST = 0x37;
static const uint8_t BMI160_CMD_EXTMODE_EN_MIDDLE = 0x9A;
static const uint8_t BMI160_CMD_EXTMODE_EN_LAST = 0xc0;

// Extended mode register; see Bosch Android driver
static const uint8_t BMI160_REG_EXT_MODE = 0x7F;
static const uint8_t BMI160_EXT_MODE_PAGING_EN = 0x80;
static const uint8_t BMI160_EXT_MODE_TARGET_PAGE_1 = 0x10;

// Constants
static const uint8_t BMI160_CHIP_ID = 0xD1;

static const uint8_t BMI160_REG_MASK = 0x7F;  // Register address is 7 bits wide
static const uint8_t BMI160_READ_FLAG = 0x80;

/*
 * Register Definitions
 */

// ACC_CONF
static const uint8_t BMI160_ACC_CONF_ACC_US_MASK = 0x1;
static const uint8_t BMI160_ACC_CONF_ACC_US_SHIFT = 7;
static const uint8_t BMI160_ACC_CONF_ACC_BWP_SHIFT = 4;
static const uint8_t BMI160_ACC_CONF_ACC_BWP_MASK = 0x7;
static const uint8_t BMI160_ACC_CONF_ACC_ODR_SHIFT = 0;
static const uint8_t BMI160_ACC_CONF_ACC_ODR_MASK = 0xf;

// Hz = 100 / 2 ^ (8 - ACC_ODR_VAL)
typedef enum { /* value is the interval in microseconds */
  BMI160SampleRate_12p5_HZ = 80000,
  BMI160SampleRate_25_HZ = 40000,
  BMI160SampleRate_50_HZ = 20000,
  BMI160SampleRate_100_HZ = 10000,
  BMI160SampleRate_200_HZ = 5000,
  BMI160SampleRate_400_HZ = 2500,
  BMI160SampleRate_800_HZ = 1250,
  BMI160SampleRate_1600_HZ = 625,
} BMI160SampleRate;

typedef enum { /* value matches ACC_CONF ODR setting that must be programmed */
  BMI160AccODR_12p5_HZ = 5,
  BMI160AccODR_25_HZ = 6,
  BMI160AccODR_50_HZ = 7,
  BMI160AccODR_100_HZ = 8,
  BMI160AccODR_200_HZ = 9,
  BMI160AccODR_400_HZ = 10,
  BMI160AccODR_800_HZ = 11,
  BMI160AccODR_1600_HZ = 12,
} BMI160AccODR;
// TODO: Create a better way to change the frequency

#define BMI160_ACC_CONF_ODR_RESET_VALUE_US BMI160SampleRate_100_HZ

#define BMI160_ACC_CONF_NORMAL_BODE_US_AND_BWP 0x2
#define BMI160_DEFAULT_ACC_CONF_VALUE                                   \
  ((BMI160_ACC_CONF_NORMAL_BODE_US_AND_BWP <<  BMI160_ACC_CONF_ACC_BWP_SHIFT)  | \
  (BMI160AccODR_50_HZ <<  BMI160_ACC_CONF_ACC_ODR_SHIFT))

#define BMI160_ACC_CONF_SELF_TEST_VALUE 0x2c // See 2.8.1 of bmi160 data sheet

// Values for BMI160_REG_ACC_RANGE
static const uint8_t BMI160_ACC_RANGE_2G = 0x3;
static const uint8_t BMI160_ACC_RANGE_4G = 0x5;
static const uint8_t BMI160_ACC_RANGE_8G = 0x8;
static const uint8_t BMI160_ACC_RANGE_16G = 0xC;

// STATUS
static const uint8_t BMI160_STATUS_DRDY_ACC_MASK = (1 << 7);
static const uint8_t BMI160_STATUS_DRDY_GYR_MASK = (1 << 6);
static const uint8_t BMI160_STATUS_DRDY_MAG_MASK = (1 << 5);
static const uint8_t BMI160_STATUS_NVM_RDY_MASK = (1 << 4);
static const uint8_t BMI160_STATUS_FOC_RDY_MASK = (1 << 3);
static const uint8_t BMI160_STATUS_MAG_MAN_OP_MASK = (1 << 2);
static const uint8_t BMI160_STATUS_GYR_SELF_TEST_OK_MASK = (1 << 1);
static const uint8_t BMI160_STATUS_GYR_POR_DETECTED_MASK = (1 << 0);

// INT_TAP[0]
static const uint8_t BMI160_INT_TAP_QUIET_SHIFT = 7;
static const uint8_t BMI160_INT_TAP_SHOCK_SHIFT = 6;
// bits 5:3 reserved
static const uint8_t BMI160_INT_TAP_DUR_SHIFT = 0;

// INT_TAP[1] Register Definition
// bit 7 reserved
static const uint8_t BMI160_INT_TAP_THRESH_SHIFT = 4;

// INT_MAP[0] Register
static const uint8_t BMI160_INT_MAP_FLAT_EN_MASK = (1 << 7);
static const uint8_t BMI160_INT_MAP_ORIENTATION_EN_MASK = (1 << 6);
static const uint8_t BMI160_INT_MAP_SINGLE_TAP_EN_MASK = (1 << 5);
static const uint8_t BMI160_INT_MAP_DOUBLE_TAP_EN_MASK = (1 << 4);
static const uint8_t BMI160_INT_MAP_NO_MOTION_EN_MASK = (1 << 3);
static const uint8_t BMI160_INT_MAP_ANYMOTION_EN_MASK = (1 << 2);
static const uint8_t BMI160_INT_MAP_HIGHG_EN_MASK = (1 << 1);
static const uint8_t BMI160_INT_MAP_LOWG_MASK = (1 << 0);

// INT_MAP[1] Register
static const uint8_t BMI160_INT_MAP_1_INT1_DATA_READY = (1 << 7);
static const uint8_t BMI160_INT_MAP_1_INT1_FIFO_WATERMARK = (1 << 6);
static const uint8_t BMI160_INT_MAP_1_INT1_FIFO_FULL = (1 << 5);
static const uint8_t BMI160_INT_MAP_1_INT1_PMU_TRIGGER = (1 << 4);
static const uint8_t BMI160_INT_MAP_1_INT2_DATA_READY = (1 << 3);
static const uint8_t BMI160_INT_MAP_1_INT2_FIFO_WATERMARK = (1 << 2);
static const uint8_t BMI160_INT_MAP_1_INT2_FIFO_FULL = (1 << 1);
static const uint8_t BMI160_INT_MAP_1_INT2_PMU_TRIGGER = (1 << 0);

// INT_STATUS[0]
static const uint8_t BMI160_INT_STATUS_0_FLAT_MASK = (1 << 7);
static const uint8_t BMI160_INT_STATUS_0_ORIENT_MASK = (1 << 6);
static const uint8_t BMI160_INT_STATUS_0_S_TAP_MASK = (1 << 5);
static const uint8_t BMI160_INT_STATUS_0_D_TAP_MASK = (1 << 4);
static const uint8_t BMI160_INT_STATUS_0_PMU_TRIGGER_MASK = (1 << 3);
static const uint8_t BMI160_INT_STATUS_0_ANYM_MASK = (1 << 2);
static const uint8_t BMI160_INT_STATUS_0_SIGMOT_MASK = (1 << 1);
static const uint8_t BMI160_INT_STATUS_0_STEP_INT_MASK = (1 << 0);

// INT_STATUS[1]
static const uint8_t BMI160_INT_STATUS_1_NOMO_MASK = (1 << 7);
static const uint8_t BMI160_INT_STATUS_1_FWM_MASK = (1 << 6);
static const uint8_t BMI160_INT_STATUS_1_FFULL_MASK = (1 << 5);
static const uint8_t BMI160_INT_STATUS_1_DRDY_MASK = (1 << 4);
static const uint8_t BMI160_INT_STATUS_1_LOWG_MASK = (1 << 3);
static const uint8_t BMI160_INT_STATUS_1_HIGHG_Z_MASK = (1 << 2);
// bit 1 & 0 reserved

// INT_STATUS[2]
static const uint8_t BMI160_INT_STATUS_2_TAP_SIGN = (1 << 7);
static const uint8_t BMI160_INT_STATUS_2_TAP_FIRST_Z = (1 << 6);
static const uint8_t BMI160_INT_STATUS_2_TAP_FIRST_Y = (1 << 5);
static const uint8_t BMI160_INT_STATUS_2_TAP_FIRST_X = (1 << 4);
static const uint8_t BMI160_INT_STATUS_2_ANYM_SIGN = (1 << 3);
static const uint8_t BMI160_INT_STATUS_2_ANYM_FIRST_Z = (1 << 2);
static const uint8_t BMI160_INT_STATUS_2_ANYM_FIRST_Y = (1 << 1);
static const uint8_t BMI160_INT_STATUS_2_ANYM_FIRST_X = (1 << 0);

// INT_EN[0]
static const uint8_t BMI160_INT_EN_0_FLAT_EN = (1 << 7);
static const uint8_t BMI160_INT_EN_0_ORIENT_EN = (1 << 6);
static const uint8_t BMI160_INT_EN_0_S_TAP_EN = (1 << 5);
static const uint8_t BMI160_INT_EN_0_D_TAP_EN = (1 << 4);
// bit 3 reserved
static const uint8_t BMI160_INT_EN_0_ANYMOTION_Z_EN = (1 << 2);
static const uint8_t BMI160_INT_EN_0_ANYMOTION_Y_EN = (1 << 1);
static const uint8_t BMI160_INT_EN_0_ANYMOTION_X_EN = (1 << 0);

// INT_EN[1]
// bit 7 reserved
static const uint8_t BMI160_INT_EN_1_FWM_EN = (1 << 6);
static const uint8_t BMI160_INT_EN_1_FFUL_EN = (1 << 5);
static const uint8_t BMI160_INT_EN_1_DRDY_EN = (1 << 4);
static const uint8_t BMI160_INT_EN_1_LOW_EN = (1 << 3);
static const uint8_t BMI160_INT_EN_1_HIGHZ_EN = (1 << 2);
static const uint8_t BMI160_INT_EN_1_HIGHY_EN = (1 << 1);
static const uint8_t BMI160_INT_EN_1_HIGHX_EN = (1 << 0);

// FIFO_CONFIG[0]
static const uint8_t BMI160_FIFO_CONFIG_0_FWM_SHIFT = 0;
static const uint8_t BMI160_FIFO_CONFIG_0_FWM_MASK = 0xff;

#define BMI160_FIFO_LEN_BYTES            1024
#define BMI160_FIFO_WM_UNIT_BYTES        4

// FIFO_CONFIG[1]
static const uint8_t BMI160_FIFO_CONFIG_1_GYR_EN = (1 << 7);
static const uint8_t BMI160_FIFO_CONFIG_1_ACC_EN = (1 << 6);
static const uint8_t BMI160_FIFO_CONFIG_1_MAG_EN = (1 << 5);
static const uint8_t BMI160_FIFO_CONFIG_1_HDR_EN = (1 << 4);
static const uint8_t BMI160_FIFO_CONFIG_1_TAG_INT1_EN = (1 << 3);
static const uint8_t BMI160_FIFO_CONFIG_1_TAG_INT2_EN = (1 << 2);
static const uint8_t BMI160_FIFO_CONFIG_1_TIME_EN = (1 << 1);
// bit 0 reserved

// INT_MOTION[0]
static const uint8_t BMI160_INT_MOTION_1_ANYM_DUR_SHIFT = 0;
static const uint8_t BMI160_INT_MOTION_1_ANYM_DUR_MASK = 0x3;
static const uint8_t BMI160_INT_MOTION_1_SLOWM_SHIFT = 2;
static const uint8_t BMI160_INT_MOTION_1_SLOWM_MASK = 0xfc;

// INT_MOTION[1]
static const uint8_t BMI160_INT_MOTION_1_ANYM_THRESH_SHIFT = 0;
static const uint8_t BMI160_INT_MOTION_1_ANYM_THRESH_MASK = 0xff;
