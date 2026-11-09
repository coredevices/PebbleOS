/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/regulator.h>
#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_aw86225 AW86225
 * @ingroup drivers_vibe
 * @brief Awinic AW86225 @ref drivers_vibe driver.
 *
 * Plays a sine cycle at the LRA's resonant frequency from the chip's RAM in an infinite loop,
 * with the gain compensated for the supply voltage.
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
  /** Nominal resonant frequency of the LRA, in Hz. */
  uint16_t f0_hz;
  /** Accepted deviation of the measured resonant frequency, in Hz. */
  uint16_t f0_tolerance_hz;
  /** Rated drive voltage of the LRA, in mV RMS. */
  uint16_t rated_mv;
  /** Measured output as a percentage of the nominal model of the chip. */
  uint8_t gain_percent;
  /** Supply, or NULL. */
  const struct pbl_regulator *supply;
};

/** @cond INTERNAL_HIDDEN */
/** Output of a full-scale sample, in mV, given the output gain in percent. */
#define PBL_AW86225_FULL_SCALE_MV(gain_percent) (4200U * (gain_percent) / 100U)
/** @endcond */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe_ops pbl_aw86225_ops;
/** @endcond */

/**
 * @brief Define an AW86225.
 *
 * @param sym Symbol of the motor driver.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _reset Reset line, from PBL_GPIO().
 * @param _f0_hz Nominal resonant frequency of the LRA, in Hz.
 * @param _f0_tolerance_hz Accepted deviation of the measured resonant frequency, in Hz.
 * @param _rated_uv Rated drive voltage of the LRA, in uV RMS.
 * @param _gain_percent Measured output as a percentage of the nominal model of the chip, 50 to
 *        100; 100 applies no correction.
 * @param _supply Supply, or NULL.
 */
#define PBL_AW86225_DEFINE(sym, _bus, _addr, _reset, _f0_hz, _f0_tolerance_hz, _rated_uv,        \
                           _gain_percent, _supply)                                               \
  static_assert((_gain_percent) >= 50 && (_gain_percent) <= 100, "output gain of 50 to 100%");   \
  static_assert((_rated_uv) / 1000U * 1414U / 1000U <= PBL_AW86225_FULL_SCALE_MV(_gain_percent), \
                "rated voltage exceeds the full-scale output");                                  \
  PBL_VIBE_STATE_DEFINE(sym);                                                                    \
  static struct pbl_aw86225_state sym##_aw86225_state = {.strength = VIBE_STRENGTH_MAX};         \
  PBL_DEVICE_INSTANCE(sym, struct pbl_aw86225, struct pbl_vibe, vibe) {                          \
    .vibe = PBL_VIBE_INIT(sym, "aw86225", &(_bus)->dev, &pbl_aw86225_ops, VIBE_STRENGTH_OFF),    \
    .state = &sym##_aw86225_state, .i2c = PBL_I2C_DEV(_bus, _addr), .reset = _reset,             \
    .f0_hz = (_f0_hz), .f0_tolerance_hz = (_f0_tolerance_hz), .rated_mv = (_rated_uv) / 1000U,   \
    .gain_percent = (_gain_percent), .supply = (_supply),                                        \
  }

/** @} */
