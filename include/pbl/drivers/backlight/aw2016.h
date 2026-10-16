/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/i2c.h>

/**
 * @defgroup drivers_backlight_aw2016 AW2016
 * @ingroup drivers_backlight
 * @brief Awinic AW2016 RGB LED controller @ref drivers_backlight driver.
 *
 * @code{.c}
 * PBL_AW2016_DEFINE(s_aw2016, &s_i2c.bus, 0x64);
 *
 * const struct pbl_backlight *const BACKLIGHT = &s_aw2016.backlight;
 * @endcode
 * @{
 */

/** @brief AW2016 RGB LED controller. */
struct pbl_aw2016 {
  /** Backlight, child of the I2C bus. */
  struct pbl_backlight backlight;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_aw2016_ops;
/** @endcond */

/**
 * @brief Define an AW2016.
 *
 * @param sym Symbol of the instance; the backlight is @c sym.backlight.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_AW2016_DEFINE(sym, _bus, _addr)                                        \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                                 \
  const struct pbl_aw2016 sym = {                                                  \
    .backlight = PBL_BACKLIGHT_INIT(sym, "aw2016", &(_bus)->dev, &pbl_aw2016_ops), \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                               \
  };                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.backlight.dev)

/** @} */
