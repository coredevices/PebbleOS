/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bma255.h"

#include "util/attributes.h"

#include <inttypes.h>

// Read & Write flags to be masked onto register addresses for raw spi transactions
static const uint8_t BMA255_READ_FLAG  = 0x80;
static const uint8_t BMA255_WRITE_FLAG = 0x00;

// BMI255 Register Map
typedef enum {
  BMA255Register_BGW_CHIP_ID = 0x00,
  BMA255Register_ACCD_X_LSB = 0x02,
  BMA255Register_ACCD_X_MSB = 0x03,
  BMA255Register_ACCD_Y_LSB = 0x04,
  BMA255Register_ACCD_Y_MSB = 0x05,
  BMA255Register_ACCD_Z_LSB = 0x06,
  BMA255Register_ACCD_Z_MSB = 0x07,
  BMA255Register_ACCD_TEMP = 0x08,
  BMA255Register_INT_STATUS_0 = 0x09,
  BMA255Register_INT_STATUS_1 = 0x0a,
  BMA255Register_INT_STATUS_2 = 0x0b,
  BMA255Register_INT_STATUS_3 = 0x0c,
  BMA255Register_FIFO_STATUS = 0x0e,
  BMA255Register_PMU_RANGE = 0x0f,
  BMA255Register_PMU_BW = 0x10,
  BMA255Register_PMU_LPW = 0x11,
  BMA255Register_PMU_LOW_POWER = 0x12,
  BMA255Register_ACCD_HBW = 0x13,
  BMA255Register_BGW_SOFTRESET = 0x14,
  BMA255Register_INT_EN_0 = 0x16,
  BMA255Register_INT_EN_1 = 0x17,
  BMA255Register_INT_EN_2 = 0x18,
  BMA255Register_INT_MAP_0 = 0x19,
  BMA255Register_INT_MAP_1 = 0x1a,
  BMA255Register_INT_MAP_2 = 0x1b,
  BMA255Register_INT_SRC = 0x1e,
  BMA255Register_INT_OUT_CTRL = 0x20,
  BMA255Register_INT_RST_LATCH = 0x21,
  BMA255Register_INT_0 = 0x22,
  BMA255Register_INT_1 = 0x23,
  BMA255Register_INT_2 = 0x24,
  BMA255Register_INT_3 = 0x25,
  BMA255Register_INT_4 = 0x26,
  BMA255Register_INT_5 = 0x27,
  BMA255Register_INT_6 = 0x28,
  BMA255Register_INT_7 = 0x29,
  BMA255Register_INT_8 = 0x2a,
  BMA255Register_INT_9 = 0x2b,
  BMA255Register_INT_a = 0x2c,
  BMA255Register_INT_b = 0x2d,
  BMA255Register_INT_c = 0x2e,
  BMA255Register_INT_d = 0x2f,
  BMA255Register_FIFO_CONFIG_0 = 0x30,
  BMA255Register_PMU_SELFTEST = 0x32,
  BMA255Register_TRIM_NVM_CTRL = 0x33,
  BMA255Register_BGW_SPI3_WDT = 0x34,
  BMA255Register_OFC_CTRL = 0x36,
  BMA255Register_OFC_SETTINGS = 0x37,
  BMA255Register_OFC_OFFSET_X = 0x38,
  BMA255Register_OFC_OFFSET_Y = 0x39,
  BMA255Register_OFC_OFFSET_Z = 0x3a,
  BMA255Register_TRIM_GPO0 = 0x3b,
  BMA255Register_TRIM_GP1 = 0x3c,
  BMA255Register_FIFO_CONFIG_1 = 0x3e,
  BMA255Register_FIFO_DATA = 0x3f,

  BMA255Register_EXTENDED_MEMORY_MAP = 0x35,
  BMA255Register_TEMPERATURE_SENSOR_CTRL = 0x4f,
} BMA255Register;

static const uint8_t BMA255_EXTENDED_MEMORY_MAP_OPEN = 0xca;
static const uint8_t BMA255_EXTENDED_MEMORY_MAP_CLOSE = 0x0a;

static const uint8_t BMA255_TEMPERATURE_SENSOR_DISABLE = 0x0;

static const uint8_t BMA255_CHIP_ID = 0xfa;
static const uint8_t BMA255_ACC_CONF_PMU_BW_MASK = 0x1f;
static const uint8_t BMA255_SOFT_RESET_VALUE = 0xb6;

static const uint8_t BMA255_FIFO_MODE_SHIFT = 6;
static const uint8_t BMA255_FIFO_MODE_MASK = 0xc0;

static const uint8_t BMA255_FIFO_DATA_SEL_SHIFT = 0;
static const uint8_t BMA255_FIFO_DATA_SEL_MASK = 0x03;

static const uint8_t BMA255_INT_STATUS_0_SLOPE_MASK = (1 << 2);

static const uint8_t BMA255_INT_STATUS_2_SLOPE_SIGN = (1 << 3);
static const uint8_t BMA255_INT_STATUS_2_SLOPE_FIRST_X = (1 << 0);
static const uint8_t BMA255_INT_STATUS_2_SLOPE_FIRST_Y = (1 << 1);
static const uint8_t BMA255_INT_STATUS_2_SLOPE_FIRST_Z = (1 << 2);

static const uint8_t BMA255_INT_MAP_1_INT2_FIFO_FULL = (0x1 << 5);
static const uint8_t BMA255_INT_MAP_1_INT2_FIFO_WATERMARK = (0x1 << 6);
static const uint8_t BMA255_INT_MAP_1_INT2_DATA = (0x1 << 7);

static const uint8_t BMA255_INT_MAP_0_INT1_SLOPE = (0x1 << 2);

static const uint8_t BMA255_INT_EN_0_SLOPE_X_EN = (1 << 0);
static const uint8_t BMA255_INT_EN_0_SLOPE_Y_EN = (1 << 1);
static const uint8_t BMA255_INT_EN_0_SLOPE_Z_EN = (1 << 2);

static const uint8_t BMA255_INT_EN_1_DATA = (0x1 << 4);
static const uint8_t BMA255_INT_EN_1_FIFO_FULL = (0x1 << 5);
static const uint8_t BMA255_INT_EN_1_FIFO_WATERMARK = (0x1 << 6);

static const uint8_t BMA255_INT_5_SLOPE_DUR_SHIFT = 0;
static const uint8_t BMA255_INT_5_SLOPE_DUR_MASK = 0x3;

static const uint8_t BMA255_LPW_SLEEP_DUR_SHIFT = 1;
static const uint8_t BMA255_LPW_SLEEP_DUR_MASK = (0xf << 1);

static const uint8_t BMA255_LPW_POWER_SHIFT = 5;
static const uint8_t BMA255_LPW_POWER_MASK = (0x7 << 5);

static const uint8_t BMA255_LOW_POWER_SHIFT = 5;
static const uint8_t BMA255_LOW_POWER_MASK = (0x3 << 5);

typedef struct PACKED {
  uint16_t x;
  uint16_t y;
  uint16_t z;
} BMA255AccelData;

typedef enum {
  BMA255FifoMode_Bypass = 0x00,
  BMA255FifoMode_Fifo   = 0x01,
  BMA255FifoMode_Stream = 0x02,
} BMA255FifoMode;

typedef enum {
  BMA255FifoDataSel_XYZ = 0x00,
  BMA255FifoDataSel_X   = 0x01,
  BMA255FifoDataSel_Y   = 0x02,
  BMA255FifoDataSel_Z   = 0x03,
} BMA255FifoDataSel;

//! Configuration to be used to enter each of the supported power modes.
//! Make sure that the PMU_LOW_POWER register is always set prior to the PMU_LPW
//! register, as the bma255 uses this restriction.
static const struct {
  uint8_t low_power;  //!< PMU_LOW_POWER register
  uint8_t lpw;        //!< PMU_LPW register
} s_power_mode[BMA255PowerModeCount] = {
  [BMA255PowerMode_Normal]      = { .low_power = 0x0, .lpw = 0x0 },
  [BMA255PowerMode_Suspend]     = { .low_power = 0x0, .lpw = 0x4 },
  [BMA255PowerMode_Standby]     = { .low_power = 0x2, .lpw = 0x4 },
  [BMA255PowerMode_DeepSuspend] = { .low_power = 0x0, .lpw = 0x1 },
  [BMA255PowerMode_LowPower1]   = { .low_power = 0x1, .lpw = 0x2 },
  [BMA255PowerMode_LowPower2]   = { .low_power = 0x3, .lpw = 0x2 },
};

//! Configuration to be used for each ODR we will be using.
//! This involves some native bma255 bandwidth and a tsleep value.
//! Errata states that at 4G sensitivity, we need to run at a bandwidth of 250HZ or lower.
//! See the discussion around \ref BMA255ODR for more information.
static const struct {
  BMA255Bandwidth bw;
  BMA255SleepDuration tsleep;
} s_odr_settings[BMA255ODRCount] = {
  [BMA255ODR_1HZ]   = { BMA255Bandwidth_250HZ, BMA255SleepDuration_1000ms },
  [BMA255ODR_10HZ]  = { BMA255Bandwidth_250HZ, BMA255SleepDuration_100ms  },
  [BMA255ODR_19HZ]  = { BMA255Bandwidth_250HZ, BMA255SleepDuration_50ms   },
  [BMA255ODR_83HZ]  = { BMA255Bandwidth_250HZ, BMA255SleepDuration_10ms   },
  [BMA255ODR_125HZ] = { BMA255Bandwidth_250HZ, BMA255SleepDuration_6ms    },
  [BMA255ODR_166HZ] = { BMA255Bandwidth_250HZ, BMA255SleepDuration_4ms    },
  [BMA255ODR_250HZ] = { BMA255Bandwidth_250HZ, BMA255SleepDuration_2ms    },
};
