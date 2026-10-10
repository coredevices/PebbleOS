/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_regulator Regulator
 * @ingroup drivers
 * @brief Power rail class.
 *
 * A power rail is a struct pbl_regulator device. Consumers enable and disable it with a use
 * count; a rail marked always-on is switched on at init and stays on, and any other rail starts
 * off.
 *
 * @code{.c}
 * pbl_regulator_enable(dev->vdd);
 * // use the device
 * pbl_regulator_disable(dev->vdd);
 * @endcode
 * @{
 */

struct pbl_regulator;

/** @brief Regulator driver operations. */
struct pbl_regulator_ops {
  /** Optional. Apply the rail's static configuration. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_regulator *reg);
  /** Switch the rail on. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_regulator *reg);
  /** Switch the rail off. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_regulator *reg);
};

/** @cond INTERNAL_HIDDEN */
struct pbl_regulator_state {
  struct pbl_mutex mutex;
  uint16_t use_count;
};
/** @endcond */

/** @brief A power rail. */
struct pbl_regulator {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_regulator_ops *ops;
  /** Runtime state. */
  struct pbl_regulator_state *state;
  /** Switched on at init and never off. */
  bool always_on;
};

/**
 * @brief Define the class state of regulator @p sym, for the @c PBL_*_REGULATOR_DEFINE() macro
 * of a driver.
 *
 * @param sym Symbol of the regulator instance.
 */
#define PBL_REGULATOR_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);         \
  static struct pbl_regulator_state sym##_regulator_state

/**
 * @brief Initializer for the struct pbl_regulator of regulator @p sym.
 *
 * @param sym Symbol of the regulator instance, with its state defined by
 * PBL_REGULATOR_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _always_on Switched on at init and never off.
 */
#define PBL_REGULATOR_INIT(sym, _name, _parent, _ops, _always_on)              \
  {                                                                            \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_regulator_dev_init, _parent, NULL), \
    .ops = (_ops),                                                             \
    .state = &sym##_regulator_state,                                           \
    .always_on = (_always_on),                                                 \
  }

/**
 * @brief Device init of every regulator: applies the driver's init, then switches the rail on if
 * always-on, off otherwise.
 *
 * @param dev Regulator device.
 * @return 0 or a negative errno.
 */
int pbl_regulator_dev_init(const struct pbl_device *dev);

/**
 * @brief Take a reference on a rail, switching it on for the first user.
 *
 * @param reg Regulator.
 * @return 0 or a negative errno.
 */
int pbl_regulator_enable(const struct pbl_regulator *reg);

/**
 * @brief Drop a reference on a rail, switching it off after the last user.
 *
 * Must balance a successful pbl_regulator_enable().
 *
 * @param reg Regulator.
 * @return 0 or a negative errno.
 */
int pbl_regulator_disable(const struct pbl_regulator *reg);

/**
 * @brief Check whether a rail is on.
 *
 * @param reg Regulator.
 * @return True if the rail is always-on or has users.
 */
bool pbl_regulator_is_enabled(const struct pbl_regulator *reg);

/** @} */
