/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/sensor.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_sensor_lis2dw12 LIS2DW12
 * @ingroup drivers_sensor
 * @brief ST LIS2DW12 accelerometer.
 *
 * @code{.c}
 * PBL_LIS2DW12_DEFINE(s_lis2dw12, &s_i2c1, 0x19, PBL_SENSOR_MOUNT_IDENTITY,
 *                     .int1 = ...,
 *                     .int1_in = ...);
 *
 * const struct pbl_sensor *const ACCEL = &s_lis2dw12;
 * @endcode
 * @{
 */

/** @brief FIFO depth in samples. */
#define LIS2DW12_FIFO_SIZE 32
/** @brief Sample size in bytes (X, Y, Z, 16 bits each). */
#define LIS2DW12_SAMPLE_SIZE_BYTES 6
/** @brief Number of FIFO read buffers. */
#define LIS2DW12_NUM_BUFS 2

/** @brief LIS2DW12 driver state, owned by the driver. */
struct pbl_lis2dw12_state {
  /** Shake detection requested. */
  bool shake_detection_enabled;
  /** Double tap detection requested. */
  bool double_tap_detection_enabled;
  /** Sampling interval in microseconds, 0 when not sampling. */
  uint32_t sampling_interval_us;
  /** Samples per FIFO batch requested by the subscribers, 0 when not streaming. */
  uint8_t num_samples;
  /** Watchdog timer detecting a stalled INT1 stream. */
  struct pbl_wallclock_work int1_wdt_timer;
  /** Time of the last FIFO read. */
  uint64_t last_fifo_read_tick;
  /** Expected INT1 period in milliseconds. */
  uint32_t int1_period_ms;
  /** Number of stream recoveries performed. */
  uint32_t num_recoveries;
  /** Current wake-up (shake) threshold register value. */
  uint8_t wk_ths_curr;
  /** Consecutive watchdog passes with INT1 stuck high in shake-only mode. */
  uint8_t shake_stuck_passes;
  /** Most recent raw sample, returned by reads while streaming. */
  uint8_t last_sample[LIS2DW12_SAMPLE_SIZE_BYTES];
  /** Time of @ref last_sample, in microseconds of uptime. */
  uint64_t last_sample_timestamp_us;
  /** @ref last_sample holds a sample. */
  bool last_sample_valid;
  /** A stream recovery is queued. */
  bool recovery_pending;
  /** Wake-up condition currently asserted, to report a shake once per assertion. */
  bool wu_active;
};

/** @brief LIS2DW12 accelerometer. */
struct pbl_lis2dw12 {
  /** Sensor, child of the I2C bus. */
  struct pbl_sensor sensor;
  /** Driver state. */
  struct pbl_lis2dw12_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /**
   * INT1 interrupt line. Without one the part is only reset and left powered down, and
   * streaming and shake detection are not supported.
   */
  struct pbl_irq_spec int1;
  /** INT1 input, to read back the pad level, none without @ref int1. */
  struct pbl_gpio int1_in;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor_ops pbl_lis2dw12_ops;
/** @endcond */

/**
 * @brief Define a LIS2DW12.
 *
 * Only one instance is supported.
 *
 * @param sym Symbol of the instance, a struct pbl_sensor.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _mount Mounting, from PBL_SENSOR_MOUNT_MATRIX().
 * @param ... Designated initializers of the optional fields: @c int1 and @c int1_in.
 */
#define PBL_LIS2DW12_DEFINE(sym, _bus, _addr, _mount, ...)                                         \
  PBL_SENSOR_STREAM_STATE_DEFINE(sym, LIS2DW12_NUM_BUFS,                                           \
                                 LIS2DW12_FIFO_SIZE *LIS2DW12_SAMPLE_SIZE_BYTES);                  \
  static struct pbl_lis2dw12_state sym##_lis2dw12_state;                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_lis2dw12, struct pbl_sensor, sensor) {                       \
    .sensor = PBL_SENSOR_INIT(sym, "lis2dw12", &(_bus)->dev, &pbl_lis2dw12_ops,                    \
                              PBL_BIT(PBL_SENSOR_ACCEL), PBL_BIT(PBL_SENSOR_EVENT_SHAKE), _mount), \
    .state = &sym##_lis2dw12_state, .i2c = PBL_I2C_DEV(_bus, _addr) __VA_OPT__(, ) __VA_ARGS__,    \
  }

/** @} */
