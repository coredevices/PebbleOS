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
 * PBL_DRV2604_DEFINE(s_drv2604, &s_i2c.bus, 0x5A, PBL_GPIO(NRF5_GPIO_P0, 2, 0));
 *
 * const struct pbl_vibe *const VIBE = &s_drv2604.vibe;
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
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe_ops pbl_drv2604_ops;
/** @endcond */

/**
 * @brief Define a DRV2604.
 *
 * @param sym Symbol of the instance; the motor is @c sym.vibe.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _enable Enable line (struct pbl_gpio).
 */
#define PBL_DRV2604_DEFINE(sym, _bus, _addr, _enable)                                         \
  PBL_VIBE_STATE_DEFINE(sym);                                                                 \
  const struct pbl_drv2604 sym = {                                                            \
    .vibe = PBL_VIBE_INIT(sym, "drv2604", &(_bus)->dev, &pbl_drv2604_ops, VIBE_STRENGTH_MIN), \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                          \
    .enable = _enable,                                                                        \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.vibe.dev)

/** @} */
