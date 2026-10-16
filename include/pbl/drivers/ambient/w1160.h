/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/als.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_als_w1160 W1160
 * @ingroup drivers_als
 * @brief W1160 @ref drivers_als driver.
 *
 * While primed the sensor samples continuously and reads are plain register reads; otherwise
 * each read is a one-shot conversion.
 *
 * @code{.c}
 * static const struct pbl_als_scale s_w1160_scale = {.num = 100, .den = 483};
 *
 * PBL_W1160_DEFINE(s_w1160, &s_i2c.bus, 0x47, &s_w1160_scale);
 *
 * const struct pbl_als *const ALS = &s_w1160.als;
 * @endcode
 * @{
 */

/** @brief W1160 driver state, owned by the driver. */
struct pbl_w1160_state {
  /** Primed. */
  bool active;
  /** Continuous sampling enabled. */
  bool sampling;
  /** Time continuous sampling was enabled. */
  RtcTicks sampling_started_ticks;
  /** Last level read. */
  uint16_t cached_level;
  /** cached_level holds a reading from the current window. */
  bool cache_valid;
};

/** @brief W1160 ambient light sensor. */
struct pbl_w1160 {
  /** Ambient light sensor, child of the I2C bus. */
  struct pbl_als als;
  /** Driver state. */
  struct pbl_w1160_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_als_ops pbl_w1160_ops;
/** @endcond */

/**
 * @brief Define a W1160.
 *
 * @param sym Symbol of the instance; the sensor is @c sym.als.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _scale Lux calibration (const struct pbl_als_scale *), or nullptr.
 */
#define PBL_W1160_DEFINE(sym, _bus, _addr, _scale)                             \
  PBL_ALS_STATE_DEFINE(sym);                                                   \
  static struct pbl_w1160_state sym##_w1160_state;                             \
  const struct pbl_w1160 sym = {                                               \
    .als = PBL_ALS_INIT(sym, "w1160", &(_bus)->dev, &pbl_w1160_ops, (_scale)), \
    .state = &sym##_w1160_state,                                               \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                           \
  };                                                                           \
  PBL_DEVICE_REGISTER(sym, &sym.als.dev)

/** @} */
