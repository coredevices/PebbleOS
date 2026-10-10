/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/regular_timer.h>

/**
 * @defgroup drivers_imu_lis2dw12 LIS2DW12
 * @ingroup drivers_imu
 * @brief ST LIS2DW12 accelerometer driver board configuration.
 * @{
 */

/** @brief FIFO depth in samples. */
#define LIS2DW12_FIFO_SIZE 32
/** @brief Sample size in bytes (X, Y, Z, 16 bits each). */
#define LIS2DW12_SAMPLE_SIZE_BYTES 6

/** @brief LIS2DW12 driver state, owned by the driver. */
typedef struct LIS2DW12State {
  /** Driver initialized. */
  bool initialized;
  /** Watch mounted rotated 180 degrees: X and Y are inverted. */
  bool rotated;
  /** Shake detection requested. */
  bool shake_detection_enabled;
  /** Double tap detection requested. */
  bool double_tap_detection_enabled;
  /** Sampling interval in microseconds, 0 when not sampling. */
  uint32_t sampling_interval_us;
  /** Samples per FIFO batch requested by the subscribers, 0 when not streaming. */
  uint8_t num_samples;
  /** Raw FIFO read buffer, filled with i2c_read_register_block_dma(). */
  uint8_t raw_sample_buf[DCACHE_ROUND_UP(
      LIS2DW12_FIFO_SIZE * LIS2DW12_SAMPLE_SIZE_BYTES)] PBL_ALIGNED(DCACHE_LINE_SIZE_MAX);
  /** Watchdog timer detecting a stalled INT1 stream. */
  RegularTimerInfo int1_wdt_timer;
  /** Time of the last FIFO read. */
  RtcTicks last_fifo_read_tick;
  /** Expected INT1 period in milliseconds. */
  uint32_t int1_period_ms;
  /** Number of stream recoveries performed. */
  uint32_t num_recoveries;
  /** Current wake-up (shake) threshold register value. */
  uint8_t wk_ths_curr;
  /** Consecutive watchdog passes with INT1 stuck high in shake-only mode. */
  uint8_t shake_stuck_passes;
  /** Most recent sample, returned by peeks while streaming. */
  AccelDriverSample last_sample;
  /** @ref last_sample holds a sample. */
  bool last_sample_valid;
  /** A stream recovery is queued. */
  bool recovery_pending;
  /** Wake-up condition currently asserted, to report a shake once per assertion. */
  bool wu_active;
} LIS2DW12State;

/** @brief LIS2DW12 board configuration. */
typedef struct LIS2DW12Config {
  /** Driver state. */
  LIS2DW12State *state;
  /** I2C device. */
  I2CSlavePort i2c;
  /** INT1 interrupt line. */
  ExtiConfig int1;
  /** INT1 input, to read back the pad level. */
  struct pbl_gpio int1_in;
  /** Sensor axis feeding each watch axis (0: X, 1: Y, 2: Z). */
  uint8_t axis_map[3];
  /** Direction of each watch axis: 1 or -1. */
  int8_t axis_dir[3];
} LIS2DW12Config;

/** @} */
