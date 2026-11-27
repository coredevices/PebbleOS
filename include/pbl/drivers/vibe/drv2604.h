/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_drv2604 DRV2604
 * @ingroup drivers_vibe
 * @brief TI DRV2604 @ref drivers_vibe driver, driving an LRA in real-time playback mode.
 *
 * @code{.c}
 * PBL_DRV2604_DEFINE(s_drv2604, &s_i2c, 0x5A, PBL_GPIO(&dt_gpio0, 2, 0), DRV260X_LRA_MODE, 1384,
 *                    3009, false);
 *
 * const struct pbl_vibe *const VIBE = &s_drv2604;
 * @endcode
 * @{
 */

/** @brief DRV2604 haptic driver. */
struct pbl_drv2604 {
  /** Vibration motor, child of the I2C bus. */
  struct pbl_vibe vibe;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Enable line. */
  struct pbl_gpio enable;
  /** Actuator, DRV260X_LRA_MODE, DRV260X_LRA_NO_CAL_MODE or DRV260X_ERM_MODE. */
  uint8_t mode;
  /** Rated voltage register value. */
  uint8_t rated_voltage;
  /** Overdrive clamp register value. */
  uint8_t od_clamp;
  /** Drive the LRA open loop, at its rated frequency, rather than tracking its resonance. */
  bool lra_open_loop;
};

/** @cond INTERNAL_HIDDEN */
// Rated and overdrive voltage registers from millivolts, as Linux computes them
#define DRV2604_VOLTAGE_REG(_mv) ((uint8_t)((_mv) * 255U / 5600U))
/** @endcond */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe_ops pbl_drv2604_ops;
/** @endcond */

/**
 * @brief Define a DRV2604.
 *
 * @param sym Symbol of the instance, a struct pbl_vibe; the motor is @c sym.vibe.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _enable Enable line (struct pbl_gpio).
 * @param _mode Actuator, from dt-bindings/input/ti-drv260x.h.
 * @param _rated_mv Rated voltage of the actuator, in millivolts.
 * @param _overdrive_mv Overdrive voltage of the actuator, in millivolts.
 * @param _lra_open_loop Whether to drive an LRA open loop.
 */
#define PBL_DRV2604_DEFINE(sym, _bus, _addr, _enable, _mode, _rated_mv, _overdrive_mv,        \
                           _lra_open_loop)                                                    \
  PBL_VIBE_STATE_DEFINE(sym);                                                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_drv2604, struct pbl_vibe, vibe) {                       \
    .vibe = PBL_VIBE_INIT(sym, "drv2604", &(_bus)->dev, &pbl_drv2604_ops, VIBE_STRENGTH_MIN), \
    .i2c = PBL_I2C_DEV(_bus, _addr), .enable = _enable, .mode = (_mode),                      \
    .rated_voltage = DRV2604_VOLTAGE_REG(_rated_mv),                                          \
    .od_clamp = DRV2604_VOLTAGE_REG(_overdrive_mv), .lra_open_loop = (_lra_open_loop),        \
  }

/** @} */
