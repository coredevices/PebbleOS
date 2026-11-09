/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/drivers/als.h>
#include <pbl/drivers/i2c.h>

/**
 * @defgroup drivers_als_opt3001 OPT3001
 * @ingroup drivers_als
 * @brief TI OPT3001 @ref drivers_als driver.
 *
 * @code{.c}
 * PBL_OPT3001_DEFINE(s_opt3001, &s_i2c, 0x44);
 *
 * const struct pbl_als *const ALS = &s_opt3001;
 * @endcode
 * @{
 */

/** @brief OPT3001 ambient light sensor. */
struct pbl_opt3001 {
  /** Ambient light sensor, child of the I2C bus. */
  struct pbl_als als;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_als_ops pbl_opt3001_ops;
/** @endcond */

/**
 * @brief Define an OPT3001.
 *
 * @param sym Symbol of the instance, a struct pbl_als.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_OPT3001_DEFINE(sym, _bus, _addr)                                      \
  PBL_ALS_STATE_DEFINE(sym);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_opt3001, struct pbl_als, als) {             \
    .als = PBL_ALS_INIT(sym, "opt3001", &(_bus)->dev, &pbl_opt3001_ops, nullptr), \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                              \
  }

/** @} */
