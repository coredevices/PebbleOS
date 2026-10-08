/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/regular_timer.h>

/**
 * @defgroup drivers_imu IMU
 * @ingroup drivers
 * @brief Board configuration of inertial and magnetic sensor drivers.
 *
 * Accelerometer drivers implement the accelerometer driver interface and the magnetometer
 * driver implements @ref drivers_mag. The headers here describe what a board provides to each
 * of them: a configuration with bus, interrupt and axis mapping, and storage for the driver
 * state.
 *
 * @code{.c}
 * static LSM6DSOState s_lsm6dso_state;
 *
 * static const LSM6DSOConfig s_lsm6dso = {
 *   .state = &s_lsm6dso_state,
 *   .i2c = ...,
 *   .int1 = ...,
 *   .int1_in = ...,
 *   .axis_map = {0, 1, 2},
 *   .axis_dir = {1, 1, 1},
 * };
 * @endcode
 */

/**
 * @defgroup drivers_imu_lsm6dso LSM6DSO
 * @ingroup drivers_imu
 * @brief ST LSM6DSO accelerometer driver board configuration.
 * @{
 */

/** @brief Accelerometer sample size in bytes (X, Y, Z, 16 bits each). */
#define LSM6DSO_SAMPLE_SIZE_BYTES 6
/** @brief FIFO word size in bytes as read from FIFO_DATA_OUT (tag byte plus sample). */
#define LSM6DSO_FIFO_WORD_SIZE_BYTES 7
/** @brief FIFO watermark in samples (the FIFO holds up to 512). */
#define LSM6DSO_FIFO_THRESHOLD 128
/** @brief Read buffer capacity in samples, sized to the watermark. */
#define LSM6DSO_FIFO_SIZE LSM6DSO_FIFO_THRESHOLD

/** @brief LSM6DSO driver state, owned by the driver. */
typedef struct LSM6DSOState {
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
  uint16_t num_samples;
  /** Raw FIFO read buffer, filled with i2c_read_register_block_dma(). */
  uint8_t raw_sample_buf[DCACHE_ROUND_UP(
      LSM6DSO_FIFO_SIZE * LSM6DSO_FIFO_WORD_SIZE_BYTES)] PBL_ALIGNED(DCACHE_LINE_SIZE_MAX);
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
  /** A stream recovery is queued. */
  bool recovery_pending;
  /** Wake-up condition currently asserted, to report a shake once per assertion. */
  bool wu_active;
  /** The INT1 servicing pass was requeued while the pad stayed high. */
  bool int1_requeued;
} LSM6DSOState;

/** @brief LSM6DSO board configuration. */
typedef struct LSM6DSOConfig {
  /** Driver state. */
  LSM6DSOState *state;
  /** I2C device. */
  I2CSlavePort i2c;
  /** INT1 interrupt line. */
  ExtiConfig int1;
  /** INT1 input, to read back the pad level. */
  InputConfig int1_in;
  /** Sensor axis feeding each watch axis (0: X, 1: Y, 2: Z). */
  uint8_t axis_map[3];
  /** Direction of each watch axis: 1 or -1. */
  int8_t axis_dir[3];
} LSM6DSOConfig;

/** @} */
