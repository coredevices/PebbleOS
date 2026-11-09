/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_regulator_fixed Fixed regulators
 * @ingroup drivers_regulator
 * @brief @ref drivers_regulator driver for a rail switched by a GPIO, as a load switch.
 *
 * The enable level is set by @c _active_high, whatever the GPIO flags say, as the Linux
 * regulator-fixed binding does with enable-active-high.
 *
 * @code{.c}
 * PBL_REGULATOR_FIXED_DEFINE(s_vlcd, "vlcd", PBL_GPIO(&dt_gpio1, 29, 0), false, 0, false);
 * @endcode
 * @{
 */

/** @brief Fixed regulator. */
struct pbl_regulator_fixed {
  /** Regulator. */
  struct pbl_regulator reg;
  /** Enable GPIO. */
  struct pbl_gpio enable;
  /** Enabled at a high level. */
  bool active_high;
  /** Time from enabling until the rail is up, in microseconds. */
  uint32_t startup_delay_us;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_regulator_ops pbl_regulator_fixed_ops;
/** @endcond */

/**
 * @brief Define a fixed regulator.
 *
 * @param sym Symbol of the regulator.
 * @param _name Name.
 * @param _enable Enable GPIO, from PBL_GPIO().
 * @param _active_high Enabled at a high level.
 * @param _startup_delay_us Time from enabling until the rail is up, in microseconds.
 * @param _always_on Switched on at init and never off.
 */
#define PBL_REGULATOR_FIXED_DEFINE(sym, _name, _enable, _active_high, _startup_delay_us,       \
                                   _always_on)                                                 \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_regulator_fixed, struct pbl_regulator, reg) {            \
    .reg = PBL_REGULATOR_INIT(sym, _name, nullptr, &pbl_regulator_fixed_ops, _always_on),      \
    .enable = _enable, .active_high = (_active_high), .startup_delay_us = (_startup_delay_us), \
  }

/** @} */
