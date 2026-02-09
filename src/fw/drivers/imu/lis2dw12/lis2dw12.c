/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/accel.h"
#include "drivers/exti.h"
#include "drivers/i2c.h"
#include "drivers/rtc.h"
#include "drivers/gpio.h"
#include "services/imu/units.h"
#include "services/common/regular_timer.h"
#include "system/logging.h"
#include "system/status_codes.h"
#include "kernel/util/delay.h"
#include "util/math.h"

// Implementation notes:
//
// - Single-shot mode is used to perform peeking measurements
// - Low-power mode 1 (12-bit) is always used (minimum power mode)
// - ODR is limited to the [12.5, 200] Hz range
// - Shake detection uses 12.5Hz when no active sampling is ongoing
// - Wake-up duration absolute time depends on the ODR, a parameter that can
//   be changed depending on the sampling interval configuration. Value is NOT
//   adjusted automatically when ODR changes (we just have 2 bits...), so it is
//   possible to notice sensitivity changes when changing sampling interval.
// - For some reason (needs more investigation), INT1 is sometimes left HIGH
//   due to FIFO overruns, and without edge change, we cannot detect such
//   events. To mitigate this, there is a watchdog timer that re-arms FIFO
//   if no INT1 event is detected within the expected time window based on the
//   ODR and FIFO threshold.

// Time to wait after reset (us)
#define LIS2DW12_RESET_TIME_US 5

// Scale range when in 12-bit mode (low-power mode 1)
#define LIS2DW12_S12_SCALE_RANGE (1U << (12U - 1U))

// Registers
#define LIS2DW12_WHO_AM_I 0x0FU
#define LIS2DW12_UNDOC 0x17U
#define LIS2DW12_CTRL1 0x20U
#define LIS2DW12_CTRL2 0x21U
#define LIS2DW12_CTRL3 0x22U
#define LIS2DW12_CTRL4_INT1_PAD_CTRL 0x23U
#define LIS2DW12_CTRL5_INT2_PAD_CTRL 0x24U
#define LIS2DW12_CTRL6 0x25U
#define LIS2DW12_STATUS 0x27U
#define LIS2DW12_OUT_X_L 0x28U
#define LIS2DW12_FIFO_CTRL 0x2EU
#define LIS2DW12_FIFO_SAMPLES 0x2FU
#define LIS2DW12_WAKE_UP_THS 0x34U
#define LIS2DW12_WAKE_UP_DUR 0x35U
#define LIS2DW12_ALL_INT_SRC 0x3BU
#define LIS2DW12_CTRL7 0x3FU

// WHO_AM_I fields
#define LIS2DW12_WHO_AM_I_VAL 0x44U

// UNDOC fields
#define LIS2DW12_UNDOC_ADDR_PULLUP_DIS (1U << 6U)

// CTRL1 fields
#define LIS2DW12_CTRL1_LP_MODE1 (0U << 0U)
#define LIS2DW12_CTRL1_MODE_LP (0U << 2U)
#define LIS2DW12_CTRL1_MODE_SINGLE (2U << 2U)
#define LIS2DW12_CTRL1_ODR_PD (0x0U << 4U)
#define LIS2DW12_CTRL1_ODR_1HZ6_LP_ONLY (0x1U << 4U)
#define LIS2DW12_CTRL1_ODR_12HZ5 (0x2U << 4U)
#define LIS2DW12_CTRL1_ODR_25HZ (0x3U << 4U)
#define LIS2DW12_CTRL1_ODR_50HZ (0x4U << 4U)
#define LIS2DW12_CTRL1_ODR_100HZ (0x5U << 4U)
#define LIS2DW12_CTRL1_ODR_200HZ (0x6U << 4U)
#define LIS2DW12_CTRL1_ODR_400HZ_HP_ONLY (0x7U << 4U)
#define LIS2DW12_CTRL1_ODR_800HZ_HP_ONLY (0x8U << 4U)
#define LIS2DW12_CTRL1_ODR_1K6HZ_HP_ONLY (0x9U << 4U)

// CTRL2 fields
#define LIS2DW12_CTRL2_SOFT_RESET (1U << 6U)
#define LIS2DW12_CTRL2_BOOT (1U << 7U)

// CTRL3 fields
#define LIS2DW12_CTRL3_SLP_MODE_1 (1U << 0U)
#define LIS2DW12_CTRL3_SLP_MODE_SEL_SLP_MODE_1 (1U << 1U)

// CTRL4_INT1_PAD_CTRL fields
#define LIS2DW12_CTRL4_INT1_PAD_CTRL_INT1_WU (1U << 5U)
#define LIS2DW12_CTRL4_INT1_PAD_CTRL_INT1_FTH (1U << 1U)

// CTRL5_INT2_PAD_CTRL fields
#define LIS2DW12_CTRL5_INT2_PAD_CTRL_INT2_OVR (1U << 3U)

// CTRL6 fields
#define LIS2DW12_CTRL6_FS_2G (0U << 4U)
#define LIS2DW12_CTRL6_FS_4G (1U << 4U)
#define LIS2DW12_CTRL6_FS_8G (2U << 4U)
#define LIS2DW12_CTRL6_FS_16G (3U << 4U)

// STATUS fields
#define LIS2DW12_STATUS_DRDY (1U << 0U)

// FIFO_CTRL fields
#define LIS2DW12_FIFO_CTRL_FTH_POS 0U
#define LIS2DW12_FIFO_CTRL_FTH_MASK 0x1FU
#define LIS2DW12_FIFO_CTRL_FTH(val) \
  (((val) << LIS2DW12_FIFO_CTRL_FTH_POS) & LIS2DW12_FIFO_CTRL_FTH_MASK)
#define LIS2DW12_FIFO_CTRL_FIFO_MODE_BYPASS (0x0U << 5U)
#define LIS2DW12_FIFO_CTRL_FIFO_MODE_FIFO (0x1U << 5U)
#define LIS2DW12_FIFO_CTRL_FIFO_MODE_CONT (0x6U << 5U)

// FIFO_SAMPLES fields
#define LIS2DW12_FIFO_SAMPLES_DIFF_POS 0U
#define LIS2DW12_FIFO_SAMPLES_DIFF_MASK 0x3FU
#define LIS2DW12_FIFO_SAMPLES_DIFF_GET(val) \
  (((val) & LIS2DW12_FIFO_SAMPLES_DIFF_MASK) >> LIS2DW12_FIFO_SAMPLES_DIFF_POS)
#define LIS2DW12_FIFO_SAMPLES_FIFO_OVR (1U << 6U)
#define LIS2DW12_FIFO_SAMPLES_FIFO_FTH (1U << 7U)

// WAKE_UP_THS fields
#define LIS2DW12_WAKE_UP_THS_WK_THS_POS 0U
#define LIS2DW12_WAKE_UP_THS_WK_THS_MASK 0x3FU
#define LIS2DW12_WAKE_UP_THS_WK_THS(val) \
  (((val) << LIS2DW12_WAKE_UP_THS_WK_THS_POS) & LIS2DW12_WAKE_UP_THS_WK_THS_MASK)

// WAKE_UP_DUR fields
#define LIS2DW12_WAKE_UP_DUR_WAKE_DUR_POS 5U
#define LIS2DW12_WAKE_UP_DUR_WAKE_DUR_MASK 0x60U
#define LIS2DW12_WAKE_UP_DUR_WAKE_DUR(val) \
  (((val) << LIS2DW12_WAKE_UP_DUR_WAKE_DUR_POS) & LIS2DW12_WAKE_UP_DUR_WAKE_DUR_MASK)

// ALL_INT_SRC fields
#define LIS2DW12_ALL_INT_SRC_WU_IA (1U << 1U)

// CTRL7 fields
#define LIS2DW12_CTRL7_INTERRUPTS_ENABLE (1U << 5U)
#define LIS2DW12_CTRL7_INT2_ON_INT1 (1U << 6U)

////////////////////////////////////////////////////////////////////////////////
// Private
////////////////////////////////////////////////////////////////////////////////

static bool prv_lis2dw12_write(uint8_t reg, const uint8_t *data, uint16_t len) {
  bool ret;

  i2c_use(&LIS2DW12->i2c);
  ret = i2c_write_register_block(&LIS2DW12->i2c, reg, len, data);
  i2c_release(&LIS2DW12->i2c);

  return ret;
}

static bool prv_lis2dw12_read(uint8_t reg, uint8_t *data, uint16_t len) {
  bool ret;

  i2c_use(&LIS2DW12->i2c);
  ret = i2c_read_register_block(&LIS2DW12->i2c, reg, len, data);
  i2c_release(&LIS2DW12->i2c);

  return ret;
}

static int16_t prv_raw_to_s12(const uint8_t *raw) {
  uint16_t val;

  val = ((raw[0] >> 4U) & 0xFU) | (raw[1] << 4U);
  if (val & 0x0800U) {
    val |= 0xF000U;
  }

  return (int16_t)val;
}

static int16_t prv_axis_raw_mg(IMUCoordinateAxis axis, const uint8_t *raw) {
  uint8_t offset;
  int16_t val;

  offset = LIS2DW12->axis_map[axis];

  val = LIS2DW12->axis_dir[axis] *
        (prv_raw_to_s12(&raw[offset * 2U]) * (int16_t)LIS2DW12->scale_mg) /
        (int16_t)LIS2DW12_S12_SCALE_RANGE;

  if (LIS2DW12->state->rotated && (axis == AXIS_X || axis == AXIS_Y)) {
    val *= -1;
  }

  return val;
}

static void prv_raw_to_mg(const uint8_t *raw, AccelDriverSample *sample) {
  sample->x = prv_axis_raw_mg(AXIS_X, raw);
  sample->y = prv_axis_raw_mg(AXIS_Y, raw);
  sample->z = prv_axis_raw_mg(AXIS_Z, raw);
}

static uint64_t prv_get_curr_system_time_us(void) {
  time_t time_s;
  uint16_t time_ms;

  rtc_get_time_ms(&time_s, &time_ms);

  return (((uint64_t)time_s) * 1000 + time_ms) * 1000ULL;
}

static void prv_lis2dw12_read_samples(uint8_t num_samples) {
  uint64_t timestamp_us;

  if (!prv_lis2dw12_read(LIS2DW12_OUT_X_L, LIS2DW12->state->raw_sample_buf,
                         num_samples * LIS2DW12_SAMPLE_SIZE_BYTES)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read samples");
    return;
  }

  timestamp_us = prv_get_curr_system_time_us();

  for (uint8_t i = 0U; i < num_samples; ++i) {
    uint8_t *raw;
    AccelDriverSample sample;

    raw = &LIS2DW12->state->raw_sample_buf[i * LIS2DW12_SAMPLE_SIZE_BYTES];
    prv_raw_to_mg(raw, &sample);
    sample.timestamp_us = timestamp_us + i * LIS2DW12->state->sampling_interval_us;

    accel_cb_new_sample(&sample);
  }
}

static bool prv_lis2dw12_enable_fifo(uint8_t num_samples) {
  bool ret;
  uint8_t val;

  val = LIS2DW12_FIFO_CTRL_FIFO_MODE_BYPASS;
  ret = prv_lis2dw12_write(LIS2DW12_FIFO_CTRL, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write FIFO_CTRL register");
    return ret;
  }

  val = LIS2DW12_FIFO_CTRL_FTH(num_samples) | LIS2DW12_FIFO_CTRL_FIFO_MODE_CONT;
  ret = prv_lis2dw12_write(LIS2DW12_FIFO_CTRL, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write FIFO_CTRL register");
    return ret;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "FIFO enabled with threshold %" PRIu8, num_samples);

  return true;
}

static void prv_lis2dw12_int1_work_handler(void) {
  bool ret;
  uint8_t val;
  bool action_taken = false;

  if (LIS2DW12->state->num_samples > 0U) {
    ret = prv_lis2dw12_read(LIS2DW12_FIFO_SAMPLES, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not read FIFO_SAMPLES register");
      return;
    }

    if ((val & LIS2DW12_FIFO_SAMPLES_FIFO_OVR) != 0U) {
      PBL_LOG(LOG_LEVEL_WARNING, "FIFO overrun detected, re-arming");
      prv_lis2dw12_enable_fifo(LIS2DW12->state->num_samples);
      action_taken = true;
    } else if ((val & LIS2DW12_FIFO_SAMPLES_FIFO_FTH) != 0U) {
      uint8_t samples;

      samples = LIS2DW12_FIFO_SAMPLES_DIFF_GET(val);
      if (samples > 0U) {
        prv_lis2dw12_read_samples(samples);
        action_taken = true;
      }
    }
  }

  if (LIS2DW12->state->shake_detection_enabled) {
    ret = prv_lis2dw12_read(LIS2DW12_ALL_INT_SRC, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not read ALL_INT_SRC register");
      return;
    }

    if ((val & LIS2DW12_ALL_INT_SRC_WU_IA) != 0U) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Shake detected");
      // TODO: provide more info about the shake (axis, direction, etc.) or
      // refactor shake to be non-dimensional
      accel_cb_shake_detected(AXIS_Z, 0);
      action_taken = true;
    }
  }

  if (!action_taken) {
    PBL_LOG(LOG_LEVEL_WARNING, "INT1 triggered but no action taken");
  }
}

static void prv_lis2dw12_int1_irq_handler(bool *should_context_switch) {
  LIS2DW12->state->last_int1_tick = rtc_get_ticks();
  accel_offload_work_from_isr(prv_lis2dw12_int1_work_handler, should_context_switch);
}

static bool prv_configure_odr(uint32_t sampling_interval_us, bool shake_detection_enabled) {
  uint8_t val;
  bool ret;

  // If shake detection is enabled, ensure a minimum ODR of 12.5Hz (80ms)
  if (shake_detection_enabled && (sampling_interval_us == 0UL)) {
    sampling_interval_us = 80000UL;
  }

  val = LIS2DW12_CTRL1_LP_MODE1 | LIS2DW12_CTRL1_MODE_LP;

  if (sampling_interval_us == 0U) {
    val |= LIS2DW12_CTRL1_ODR_PD;
    sampling_interval_us = 0UL;
  } else if (sampling_interval_us >= 80000UL) {
    val |= LIS2DW12_CTRL1_ODR_12HZ5;
    sampling_interval_us = 80000UL;
  } else if (sampling_interval_us >= 40000UL) {
    val |= LIS2DW12_CTRL1_ODR_25HZ;
    sampling_interval_us = 40000UL;
  } else if (sampling_interval_us >= 20000UL) {
    val |= LIS2DW12_CTRL1_ODR_50HZ;
    sampling_interval_us = 20000UL;
  } else if (sampling_interval_us >= 10000UL) {
    val |= LIS2DW12_CTRL1_ODR_100HZ;
    sampling_interval_us = 10000UL;
  } else {
    val |= LIS2DW12_CTRL1_ODR_200HZ;
    sampling_interval_us = 5000UL;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Configuring ODR to %" PRIu32 " ms (%" PRIu32 " mHz)",
          sampling_interval_us / 1000UL,
          sampling_interval_us > 0UL ? 1000000000UL / sampling_interval_us : 0UL);

  ret = prv_lis2dw12_write(LIS2DW12_CTRL1, &val, 1);
  if (!ret) {
    return ret;
  }

  LIS2DW12->state->sampling_interval_us = sampling_interval_us;

  return true;
}

static bool prv_configure_int1(bool shake_detection_enabled, bool fifo_enabled) {
  bool ret;
  uint8_t ctrl4;
  uint8_t ctrl5;
  uint8_t ctrl7;

  ctrl4 = 0U;
  ctrl5 = 0U;

  if (shake_detection_enabled) {
    ctrl4 |= LIS2DW12_CTRL4_INT1_PAD_CTRL_INT1_WU;
  }

  if (fifo_enabled) {
    ctrl4 |= LIS2DW12_CTRL4_INT1_PAD_CTRL_INT1_FTH;
    ctrl5 |= LIS2DW12_CTRL5_INT2_PAD_CTRL_INT2_OVR;
  }

  ret = prv_lis2dw12_write(LIS2DW12_CTRL4_INT1_PAD_CTRL, &ctrl4, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL4_INT1_PAD_CTRL register");
    return ret;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "INT1 configured: %02" PRIx8, ctrl4);

  ret = prv_lis2dw12_write(LIS2DW12_CTRL5_INT2_PAD_CTRL, &ctrl5, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL5_INT2_PAD_CTRL register");
    return ret;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "INT2 configured: %02" PRIx8, ctrl5);

  ctrl7 = (ctrl4 == 0U && ctrl5 == 0U)
              ? 0U
              : LIS2DW12_CTRL7_INTERRUPTS_ENABLE | LIS2DW12_CTRL7_INT2_ON_INT1;
  ret = prv_lis2dw12_write(LIS2DW12_CTRL7, &ctrl7, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL7 register");
    return ret;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Enabled interrupts: %u", ctrl7 != 0U);

  return true;
}

static void prv_int1_wdt_cb(void *data) {
  RtcTicks now_tick = rtc_get_ticks();
  RtcTicks ticks_since_last_int1 = now_tick - LIS2DW12->state->last_int1_tick;
  uint32_t ms_since_last_int1 = (ticks_since_last_int1 * 1000) / RTC_TICKS_HZ;

  if (ms_since_last_int1 >= LIS2DW12->state->int1_period_ms) {
    bool ret;
    uint8_t val;

    PBL_LOG(LOG_LEVEL_WARNING, "INT1 not received in %" PRIu32 " ms", ms_since_last_int1);

    // Re-enable FIFO, and clear any event INT source
    ret = prv_lis2dw12_enable_fifo(LIS2DW12->state->num_samples);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to re-enable FIFO");
      return;
    }

    ret = prv_lis2dw12_read(LIS2DW12_ALL_INT_SRC, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not read ALL_INT_SRC register");
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Accelerometer interface
////////////////////////////////////////////////////////////////////////////////

void accel_init(void) {
  bool ret;
  uint8_t val;

  // Check device ID
  ret = prv_lis2dw12_read(LIS2DW12_WHO_AM_I, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not read WHO_AM_I register");
    return;
  }

  if (val != LIS2DW12_WHO_AM_I_VAL) {
    PBL_LOG(LOG_LEVEL_ERROR, "Unexpected id: 0x%02X!=0x%02X", val, LIS2DW12_WHO_AM_I_VAL);
    return;
  }

  // Perform a software reset (so we can rely on defaults)
  val = LIS2DW12_CTRL2_SOFT_RESET;
  ret = prv_lis2dw12_write(LIS2DW12_CTRL2, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL2 register");
    return;
  }

  delay_us(LIS2DW12_RESET_TIME_US);

  do {
    ret = prv_lis2dw12_read(LIS2DW12_CTRL2, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not read CTRL2 register");
      return;
    }
  } while ((val & LIS2DW12_CTRL2_BOOT) != 0U);

  // Disable ADDR pull-up if requested
  // NOTE: This is an undocumented register (provided by FAE)
  if (LIS2DW12->disable_addr_pullup) {
    ret = prv_lis2dw12_read(LIS2DW12_UNDOC, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to read LIS2DW12 register 0x17");
      return;
    }

    val |= LIS2DW12_UNDOC_ADDR_PULLUP_DIS;
    ret = prv_lis2dw12_write(LIS2DW12_UNDOC, &val, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to write LIS2DW12 register 0x17");
      return;
    }
  }

  // Single-data conversion via SLP_MODE_1
  val = LIS2DW12_CTRL3_SLP_MODE_SEL_SLP_MODE_1;
  ret = prv_lis2dw12_write(LIS2DW12_CTRL3, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL3 register");
    return;
  }

  // Configure scale
  switch (LIS2DW12->scale_mg) {
    case 2000U:
      val = LIS2DW12_CTRL6_FS_2G;
      break;
    case 4000U:
      val = LIS2DW12_CTRL6_FS_4G;
      break;
    case 8000U:
      val = LIS2DW12_CTRL6_FS_8G;
      break;
    case 16000U:
      val = LIS2DW12_CTRL6_FS_16G;
      break;
    default:
      PBL_LOG(LOG_LEVEL_ERROR, "Invalid scale: %" PRIu16, LIS2DW12->scale_mg);
      return;
  }

  ret = prv_lis2dw12_write(LIS2DW12_CTRL6, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL6 register");
    return;
  }

  // Configure wake-up threshold defaults
  val = LIS2DW12_WAKE_UP_DUR_WAKE_DUR(LIS2DW12->wk_dur_default);
  ret = prv_lis2dw12_write(LIS2DW12_WAKE_UP_DUR, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write WAKE_UP_DUR register");
    return;
  }

  val = LIS2DW12_WAKE_UP_THS_WK_THS(LIS2DW12->wk_ths_default);
  ret = prv_lis2dw12_write(LIS2DW12_WAKE_UP_THS, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write WAKE_UP_THS register");
    return;
  }

  LIS2DW12->state->wk_ths_curr = LIS2DW12->wk_ths_default;

  // Enable INT1 external interrupt
  exti_configure_pin(LIS2DW12->int1, ExtiTrigger_Rising, prv_lis2dw12_int1_irq_handler);
  exti_enable(LIS2DW12->int1);

  LIS2DW12->state->int1_wdt_timer.cb = prv_int1_wdt_cb;
  LIS2DW12->state->initialized = true;
}

void accel_power_up(void) {
  // Driver automatically keeps the sensor active as needed
}

void accel_power_down(void) {
  // Driver automatically keeps the sensor in lowest power mode
}

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  if (!LIS2DW12->state->initialized) {
    // Just pretend we can achieve any requested interval
    LIS2DW12->state->sampling_interval_us = interval_us;
  } else {
    // FIXME: we should technically stop and drain the FIFO here, otherwise
    // we may report existing samples in the FIFO buffer with an incorrect timestamp

    if (!prv_configure_odr(interval_us, LIS2DW12->state->shake_detection_enabled)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not configure ODR");
    }
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Set sampling interval to %" PRIu32 " us",
          LIS2DW12->state->sampling_interval_us);

  return LIS2DW12->state->sampling_interval_us;
}

uint32_t accel_get_sampling_interval(void) {
  return LIS2DW12->state->sampling_interval_us;
}

void accel_set_num_samples(uint32_t num_samples) {
  bool ret;
  uint8_t val;

  if (!LIS2DW12->state->initialized) {
    return;
  }

  // Limit to FIFO threshold
  if (num_samples > LIS2DW12->fifo_threshold) {
    num_samples = LIS2DW12->fifo_threshold;
  }

  // Disable all INT1 before changing FIFO threshold
  prv_configure_int1(false, false);

  if (num_samples == 0U) {
    // Bypass FIFO (disable)
    val = LIS2DW12_FIFO_CTRL_FIFO_MODE_BYPASS;
    if (!prv_lis2dw12_write(LIS2DW12_FIFO_CTRL, &val, 1)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not write FIFO_CTRL register");
    }

    regular_timer_remove_callback(&LIS2DW12->state->int1_wdt_timer);
  } else {
    // FIXME: we should ideally drain the FIFO here to not discard existing samples

    // Configure FIFO in CONT mode with threshold
    ret = prv_lis2dw12_enable_fifo((uint8_t)num_samples);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not enable FIFO");
      return;
    }

    LIS2DW12->state->last_int1_tick = rtc_get_ticks();
    LIS2DW12->state->int1_period_ms = (LIS2DW12->state->sampling_interval_us * num_samples) / 1000;
    regular_timer_add_multisecond_callback(&LIS2DW12->state->int1_wdt_timer,
                                           DIVIDE_CEIL(LIS2DW12->state->int1_period_ms, 1000UL));
  }

  // Re-configure INT1
  ret = prv_configure_int1(LIS2DW12->state->shake_detection_enabled, num_samples > 0U);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not configure INT1");
    return;
  }

  LIS2DW12->state->num_samples = num_samples;

  PBL_LOG(LOG_LEVEL_DEBUG, "Set number of samples to %" PRIu32, num_samples);
}

int accel_peek(AccelDriverSample *data) {
  bool ret;
  uint8_t ctrl1;
  uint8_t ctrl1_bck;
  uint8_t ctrl3;
  uint8_t status;
  uint8_t raw[LIS2DW12_SAMPLE_SIZE_BYTES];

  if (!LIS2DW12->state->initialized) {
    return E_ERROR;
  }

  // Save CTRL1
  ret = prv_lis2dw12_read(LIS2DW12_CTRL1, &ctrl1_bck, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not read CTRL1 register");
    return E_ERROR;
  }

  // Configure single mode, ODR@50Hz (recommended ODR, see DT0102 rev1)
  ctrl1 = LIS2DW12_CTRL1_MODE_SINGLE | LIS2DW12_CTRL1_ODR_50HZ;
  ret = prv_lis2dw12_write(LIS2DW12_CTRL1, &ctrl1, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL1 register");
    return E_ERROR;
  }

  // Trigger single measurement by setting SLP_MODE_1 bit
  ret = prv_lis2dw12_read(LIS2DW12_CTRL3, &ctrl3, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not read CTRL3 register");
    return E_ERROR;
  }

  ctrl3 |= LIS2DW12_CTRL3_SLP_MODE_1;
  ret = prv_lis2dw12_write(LIS2DW12_CTRL3, &ctrl3, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write CTRL3 register");
    return E_ERROR;
  }

  // Poll for data ready
  do {
    ret = prv_lis2dw12_read(LIS2DW12_STATUS, &status, 1);
    if (!ret) {
      PBL_LOG(LOG_LEVEL_ERROR, "Could not read STATUS register");
      return E_ERROR;
    }
  } while ((status & LIS2DW12_STATUS_DRDY) == 0U);

  // Read sample
  ret = prv_lis2dw12_read(LIS2DW12_OUT_X_L, raw, sizeof(raw));
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read sample");
    return E_ERROR;
  }

  // Restore CTRL1
  ret = prv_lis2dw12_write(LIS2DW12_CTRL1, &ctrl1_bck, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not restore CTRL1 register");
    return E_ERROR;
  }

  // Convert to mg and populate timestamp
  prv_raw_to_mg(raw, data);
  data->timestamp_us = prv_get_curr_system_time_us();

  return 0;
}

void accel_enable_shake_detection(bool on) {
  bool ret;
  uint8_t val;

  if (!LIS2DW12->state->initialized) {
    return;
  }

  // Configure ODR (use current interval, will be adjusted if < 12.5Hz)
  ret = prv_configure_odr(LIS2DW12->state->sampling_interval_us, on);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not configure ODR");
    return;
  }

  // Configure INT1
  ret = prv_configure_int1(on, LIS2DW12->state->num_samples > 0U);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not configure INT1");
    return;
  }

  LIS2DW12->state->shake_detection_enabled = on;

  PBL_LOG(LOG_LEVEL_DEBUG, "%s shake detection", on ? "Enabled" : "Disabled");
}

bool accel_get_shake_detection_enabled(void) {
  return LIS2DW12->state->shake_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  bool ret;
  uint8_t val;

  if (!LIS2DW12->state->initialized) {
    return;
  }

  val = LIS2DW12_WAKE_UP_THS_WK_THS(sensitivity_high ? LIS2DW12->wk_ths_min : LIS2DW12->state->wk_ths_curr);
  ret = prv_lis2dw12_write(LIS2DW12_WAKE_UP_THS, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write WAKE_UP_THS register");
    return;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Configured shake sensitivity to %s",
          sensitivity_high ? "high" : "normal");
}

void accel_set_shake_sensitivity_percent(uint8_t percent) {
  bool ret;
  uint8_t val;
  uint8_t raw;

  if (!LIS2DW12->state->initialized) {
    return;
  }

  // Reverse mapping: 0 = max sensitivity (MIN threshold), 100 = min sensitivity (MAX threshold)
  // [0, 100] -> [wk_ths_max, wk_ths_min]
  raw = LIS2DW12->wk_ths_max -
        (percent * (LIS2DW12->wk_ths_max - LIS2DW12->wk_ths_min)) / 100U;

  val = LIS2DW12_WAKE_UP_THS_WK_THS(raw);
  ret = prv_lis2dw12_write(LIS2DW12_WAKE_UP_THS, &val, 1);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not write WAKE_UP_THS register");
    return;
  }

  LIS2DW12->state->wk_ths_curr = raw;

  PBL_LOG(LOG_LEVEL_DEBUG, "Configured shake sensitivity to %" PRIu8 " (%" PRIu8 ")", percent, raw);
}

void accel_enable_double_tap_detection(bool on) {
  // TODO: Implement
  PBL_LOG(LOG_LEVEL_WARNING, "Double-tap detection not implemented");
}

bool accel_get_double_tap_detection_enabled(void) {
  // TODO: Implement
  return false;
}

void accel_set_rotated(bool rotated) {
  LIS2DW12->state->rotated = rotated;
  PBL_LOG(LOG_LEVEL_DEBUG, "Set rotated state to %s", rotated ? "true" : "false");
}
