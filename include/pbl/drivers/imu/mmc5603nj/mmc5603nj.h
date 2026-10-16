/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-FileCopyrightText: 2025 Bob Wei */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/mag.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup drivers_imu_mmc5603nj MMC5603NJ
 * @ingroup drivers_imu
 * @brief MEMSIC MMC5603NJ @ref drivers_mag driver.
 *
 * The part has no data-ready interrupt line: while sampling, the driver polls it at the output
 * data rate.
 *
 * @code{.c}
 * PBL_MMC5603NJ_DEFINE(s_mmc5603nj, &s_i2c2.bus, 0x30,
 *                      .axis_map = {1, 0, 2},
 *                      .axis_dir = {1, -1, 1});
 *
 * const struct pbl_mag *const MAG = &s_mmc5603nj.mag;
 * @endcode
 * @{
 */

/** @brief MMC5603NJ driver state, owned by the driver. */
struct pbl_mmc5603nj_state {
  /** Watch mounted rotated 180 degrees: X and Y are inverted. */
  bool rotated;
  /** Output data rate in Hz, 0 when not sampling. */
  uint8_t rate_hz;
  /** Polling timer, created on first use. */
  TimerID poll_timer;
};

/** @brief MMC5603NJ magnetometer. */
struct pbl_mmc5603nj {
  /** Magnetometer, child of the I2C bus. */
  struct pbl_mag mag;
  /** Driver state. */
  struct pbl_mmc5603nj_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Sensor axis feeding each watch axis (0: X, 1: Y, 2: Z). */
  uint8_t axis_map[3];
  /** Direction of each watch axis: 1 or -1. */
  int8_t axis_dir[3];
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_mag_ops pbl_mmc5603nj_ops;
/** @endcond */

/**
 * @brief Define an MMC5603NJ.
 *
 * Only one instance is supported.
 *
 * @param sym Symbol of the instance; the magnetometer is @c sym.mag.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param ... Designated initializers of the remaining fields: @c axis_map and @c axis_dir.
 */
#define PBL_MMC5603NJ_DEFINE(sym, _bus, _addr, ...)                          \
  PBL_MAG_STATE_DEFINE(sym);                                                 \
  static struct pbl_mmc5603nj_state sym##_mmc5603nj_state = {                \
    .poll_timer = TIMER_INVALID_ID,                                          \
  };                                                                         \
  const struct pbl_mmc5603nj sym = {                                         \
    .mag = PBL_MAG_INIT(sym, "mmc5603nj", &(_bus)->dev, &pbl_mmc5603nj_ops), \
    .state = &sym##_mmc5603nj_state,                                         \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                         \
    __VA_ARGS__,                                                             \
  };                                                                         \
  PBL_DEVICE_REGISTER(sym, &sym.mag.dev)

/** @} */
