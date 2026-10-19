/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/regular_timer.h>

#include <board/board.h>

/**
 * @defgroup drivers_imu IMU
 * @ingroup drivers
 * @brief Inertial and magnetic sensor drivers.
 *
 * Accelerometer drivers implement @ref drivers_accel and the magnetometer driver implements
 * @ref drivers_mag. The headers here describe what a board provides to each of them: bus,
 * interrupt and axis mapping.
 *
 * @code{.c}
 * PBL_LSM6DSO_DEFINE(s_lsm6dso, &s_i2c2.bus, 0x6A,
 *                    .int1 = ...,
 *                    .int1_in = ...,
 *                    .axis_map = {0, 1, 2},
 *                    .axis_dir = {1, 1, 1});
 *
 * const struct pbl_accel *const ACCEL = &s_lsm6dso.accel;
 * @endcode
 */

/**
 * @defgroup drivers_imu_lsm6dso LSM6DSO
 * @ingroup drivers_imu
 * @brief ST LSM6DSO @ref drivers_accel driver.
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
  /** Raw FIFO read buffer, filled with pbl_i2c_read_register_block_dma(). */
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

/** @brief LSM6DSO accelerometer. */
struct pbl_lsm6dso {
  /** Accelerometer, child of the I2C bus. */
  struct pbl_accel accel;
  /** Driver state. */
  LSM6DSOState *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** INT1 interrupt line. */
  struct pbl_irq_spec int1;
  /** INT1 input, to read back the pad level. */
  struct pbl_gpio int1_in;
  /** Sensor axis feeding each watch axis (0: X, 1: Y, 2: Z). */
  uint8_t axis_map[3];
  /** Direction of each watch axis: 1 or -1. */
  int8_t axis_dir[3];
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_accel_ops pbl_lsm6dso_ops;
/** @endcond */

/**
 * @brief Define an LSM6DSO.
 *
 * Only one instance is supported.
 *
 * @param sym Symbol of the instance; the accelerometer is @c sym.accel.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param ... Designated initializers of the remaining fields: @c int1, @c int1_in, @c axis_map
 *            and @c axis_dir.
 */
#define PBL_LSM6DSO_DEFINE(sym, _bus, _addr, ...)                            \
  PBL_ACCEL_STATE_DEFINE(sym);                                               \
  static LSM6DSOState sym##_lsm6dso_state;                                   \
  const struct pbl_lsm6dso sym = {                                           \
    .accel = PBL_ACCEL_INIT(sym, "lsm6dso", &(_bus)->dev, &pbl_lsm6dso_ops), \
    .state = &sym##_lsm6dso_state,                                           \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                         \
    __VA_ARGS__,                                                             \
  };                                                                         \
  PBL_DEVICE_REGISTER(sym, &sym.accel.dev)

/** @} */
