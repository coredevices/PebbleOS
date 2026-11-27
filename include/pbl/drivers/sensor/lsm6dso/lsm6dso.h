/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/sensor.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_sensor_lsm6dso LSM6DSO
 * @ingroup drivers_sensor
 * @brief ST LSM6DSO accelerometer and gyroscope; only the accelerometer is used.
 *
 * @code{.c}
 * PBL_LSM6DSO_DEFINE(s_lsm6dso, &s_i2c2, 0x6A,
 *                    PBL_SENSOR_MOUNT_MATRIX(-1, 0, 0, 0, 1, 0, 0, 0, 1),
 *                    .int1 = ...,
 *                    .int1_in = ...);
 *
 * const struct pbl_sensor *const ACCEL = &s_lsm6dso;
 * @endcode
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
/** @brief Number of FIFO read buffers. */
#define LSM6DSO_NUM_BUFS 2

/** @brief LSM6DSO driver state, owned by the driver. */
struct pbl_lsm6dso_state {
  /** Shake detection requested. */
  bool shake_detection_enabled;
  /** Double tap detection requested. */
  bool double_tap_detection_enabled;
  /** Sampling interval in microseconds, 0 when not sampling. */
  uint32_t sampling_interval_us;
  /** Samples per FIFO batch requested by the subscribers, 0 when not streaming. */
  uint16_t num_samples;
  /** Watchdog timer detecting a stalled INT1 stream. */
  struct pbl_wallclock_work int1_wdt_timer;
  /** Time of the last FIFO read. */
  uint64_t last_fifo_read_tick;
  /** Expected INT1 period in milliseconds. */
  uint32_t int1_period_ms;
  /** Number of stream recoveries performed. */
  uint32_t num_recoveries;
  /** Number of INT1 interrupts taken. */
  uint32_t num_int1_irqs;
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
};

/** @brief LSM6DSO accelerometer. */
struct pbl_lsm6dso {
  /** Sensor, child of the I2C bus. */
  struct pbl_sensor sensor;
  /** Driver state. */
  struct pbl_lsm6dso_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** INT1 interrupt line. */
  struct pbl_irq_spec int1;
  /** INT1 input, to read back the pad level. */
  struct pbl_gpio int1_in;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor_ops pbl_lsm6dso_ops;
/** @endcond */

/**
 * @brief Define an LSM6DSO.
 *
 * Only one instance is supported.
 *
 * @param sym Symbol of the instance, a struct pbl_sensor.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _mount Mounting, from PBL_SENSOR_MOUNT_MATRIX().
 * @param ... Designated initializers of the remaining fields: @c int1 and @c int1_in.
 */
#define PBL_LSM6DSO_DEFINE(sym, _bus, _addr, _mount, ...)                                          \
  PBL_SENSOR_STREAM_STATE_DEFINE(sym, LSM6DSO_NUM_BUFS,                                            \
                                 LSM6DSO_FIFO_SIZE *LSM6DSO_FIFO_WORD_SIZE_BYTES);                 \
  static struct pbl_lsm6dso_state sym##_lsm6dso_state;                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_lsm6dso, struct pbl_sensor, sensor) {                        \
    .sensor = PBL_SENSOR_INIT(sym, "lsm6dso", &(_bus)->dev, &pbl_lsm6dso_ops,                      \
                              PBL_BIT(PBL_SENSOR_ACCEL), PBL_BIT(PBL_SENSOR_EVENT_SHAKE), _mount), \
    .state = &sym##_lsm6dso_state, .i2c = PBL_I2C_DEV(_bus, _addr), __VA_ARGS__,                   \
  }

/** @} */
