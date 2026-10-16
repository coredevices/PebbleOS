/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_aw8623x AW8623X
 * @ingroup drivers_vibe
 * @brief Awinic AW8623X @ref drivers_vibe driver, driving an LRA in continuous mode.
 *
 * @code{.c}
 * PBL_AW8623X_DEFINE(s_aw8623x, &s_i2c.bus, 0x5a, PBL_GPIO(SF32LB_GPIO1, 20, PBL_GPIO_ACTIVE_LOW));
 *
 * const struct pbl_vibe *const VIBE = &s_aw8623x.vibe;
 * @endcode
 * @{
 */

/** @brief AW8623X driver state, owned by the driver. */
struct pbl_aw8623x_state {
  /** Drive strength, 0 to @ref VIBE_STRENGTH_MAX. */
  uint8_t strength;
};

/** @brief AW8623X haptic driver. */
struct pbl_aw8623x {
  /** Vibration motor, child of the I2C bus. */
  struct pbl_vibe vibe;
  /** Driver state. */
  struct pbl_aw8623x_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Reset line, active while the chip is held in reset. */
  struct pbl_gpio reset;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe_ops pbl_aw8623x_ops;
/** @endcond */

/**
 * @brief Define an AW8623X.
 *
 * @param sym Symbol of the instance; the motor is @c sym.vibe.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _reset Reset line (struct pbl_gpio).
 */
#define PBL_AW8623X_DEFINE(sym, _bus, _addr, _reset)                                          \
  PBL_VIBE_STATE_DEFINE(sym);                                                                 \
  static struct pbl_aw8623x_state sym##_aw8623x_state = {.strength = VIBE_STRENGTH_MAX};      \
  const struct pbl_aw8623x sym = {                                                            \
    .vibe = PBL_VIBE_INIT(sym, "aw8623x", &(_bus)->dev, &pbl_aw8623x_ops, VIBE_STRENGTH_OFF), \
    .state = &sym##_aw8623x_state,                                                            \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                          \
    .reset = _reset,                                                                          \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.vibe.dev)

/** @} */
