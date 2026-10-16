/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_aw86225 AW86225
 * @ingroup drivers_vibe
 * @brief Awinic AW86225 @ref drivers_vibe driver.
 *
 * Plays a sine cycle at the LRA's resonant frequency from the chip's RAM in an infinite loop,
 * with the gain compensated for the supply voltage. The LRA is described by the
 * @c CONFIG_VIBE_AW86225_* options.
 *
 * @code{.c}
 * PBL_AW86225_DEFINE(s_aw86225, &s_i2c.bus, 0x58, PBL_GPIO(SF32LB_GPIO1, 1, PBL_GPIO_ACTIVE_LOW));
 *
 * const struct pbl_vibe *const VIBE = &s_aw86225.vibe;
 * @endcode
 * @{
 */

/** @brief AW86225 driver state, owned by the driver. */
struct pbl_aw86225_state {
  /** Drive strength, 0 to @ref VIBE_STRENGTH_MAX. */
  uint8_t strength;
  /** Resonant frequency trim. */
  uint8_t trim_lra;
  /** trim_lra holds a calibration. */
  bool trim_valid;
  /** Supply voltage measured before the last playback, in mV, or 0 if not measured. */
  uint16_t vbat_mv;
  /** Playback running. */
  bool playing;
};

/** @brief AW86225 haptic driver. */
struct pbl_aw86225 {
  /** Vibration motor, child of the I2C bus. */
  struct pbl_vibe vibe;
  /** Driver state. */
  struct pbl_aw86225_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Reset line, active while the chip is held in reset. */
  struct pbl_gpio reset;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe_ops pbl_aw86225_ops;
/** @endcond */

/**
 * @brief Define an AW86225.
 *
 * @param sym Symbol of the instance; the motor is @c sym.vibe.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _reset Reset line (struct pbl_gpio).
 */
#define PBL_AW86225_DEFINE(sym, _bus, _addr, _reset)                                          \
  PBL_VIBE_STATE_DEFINE(sym);                                                                 \
  static struct pbl_aw86225_state sym##_aw86225_state = {.strength = VIBE_STRENGTH_MAX};      \
  const struct pbl_aw86225 sym = {                                                            \
    .vibe = PBL_VIBE_INIT(sym, "aw86225", &(_bus)->dev, &pbl_aw86225_ops, VIBE_STRENGTH_OFF), \
    .state = &sym##_aw86225_state,                                                            \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                          \
    .reset = _reset,                                                                          \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.vibe.dev)

/** @} */
