/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-FileCopyrightText: 2025 Bob Wei */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/sensor.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_sensor_mmc5603nj MMC5603NJ
 * @ingroup drivers_sensor
 * @brief MEMSIC MMC5603NJ magnetometer.
 *
 * The part has no data-ready interrupt line: while sampling, the driver polls it at the output
 * data rate. Reads run a single measurement when not sampling.
 *
 * @code{.c}
 * PBL_MMC5603NJ_DEFINE(s_mmc5603nj, &s_i2c2, 0x30,
 *                      PBL_SENSOR_MOUNT_MATRIX(0, -1, 0, 1, 0, 0, 0, 0, 1));
 * const struct pbl_sensor *const MAG = &s_mmc5603nj;
 * @endcode
 * @{
 */

/** @brief MMC5603NJ driver state, owned by the driver. */
struct pbl_mmc5603nj_state {
  /** Output data rate in Hz, 0 when not sampling. */
  uint8_t rate_hz;
  /** Polling work. */
  struct pbl_delayed_work poll_timer;
};

/** @brief MMC5603NJ magnetometer. */
struct pbl_mmc5603nj {
  /** Sensor, child of the I2C bus. */
  struct pbl_sensor sensor;
  /** Driver state. */
  struct pbl_mmc5603nj_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor_ops pbl_mmc5603nj_ops;
/** @endcond */

/**
 * @brief Define an MMC5603NJ.
 *
 * Only one instance is supported.
 *
 * @param sym Symbol of the instance, a struct pbl_sensor.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param ... Mounting, from PBL_SENSOR_MOUNT_MATRIX().
 */
#define PBL_MMC5603NJ_DEFINE(sym, _bus, _addr, ...)                               \
  PBL_SENSOR_STREAM_STATE_DEFINE(sym, 2, 6);                                      \
  static struct pbl_mmc5603nj_state sym##_mmc5603nj_state;                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_mmc5603nj, struct pbl_sensor, sensor) {     \
    .sensor = PBL_SENSOR_INIT(sym, "mmc5603nj", &(_bus)->dev, &pbl_mmc5603nj_ops, \
                              PBL_BIT(PBL_SENSOR_MAGN), 0U, __VA_ARGS__),         \
    .state = &sym##_mmc5603nj_state, .i2c = PBL_I2C_DEV(_bus, _addr),             \
  }

/** @} */
