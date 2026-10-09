/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/i2c.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/misc.h>

/**
 * @defgroup drivers_mfd_npm1300 nPM1300
 * @ingroup drivers
 * @brief Nordic nPM1300 PMIC core.
 *
 * The core is a struct pbl_npm1300 device on its I2C bus. It owns the register access, under a
 * lock so its cells cannot interleave read-modify-write sequences, and brings its cells up from
 * its init. Each cell is a child device in its own subsystem: the charger
 * (@ref drivers_pmic_npm1300), the GPIO port (@ref drivers_gpio_npm1300) and one regulator per
 * rail (@ref drivers_regulator_npm1300).
 *
 * @code{.c}
 * PBL_NPM1300_DEFINE(s_npm1300, &s_i2c1.bus, 0x6B);
 * PBL_NPM1300_CHARGER_DEFINE(s_npm1300_charger, &s_npm1300, &NPM1300_CONFIG,
 *                            {.peripheral = hwp_gpio1, .gpio_pin = 26});
 * PBL_NPM1300_GPIO_DEFINE(s_npm1300_gpio, &s_npm1300);
 * PBL_NPM1300_REGULATOR_DEFINE(s_ldo2, "ldo2", &s_npm1300, PBL_NPM1300_LDSW2, 3300, true, false);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_npm1300_state {
  struct pbl_mutex lock;
};
/** @endcond */

/** @brief nPM1300 core. */
struct pbl_npm1300 {
  /** Device, child of the I2C bus. */
  struct pbl_device dev;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Runtime state. */
  struct pbl_npm1300_state *state;
};

/** @cond INTERNAL_HIDDEN */
int pbl_npm1300_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the core.
 *
 * @param sym Symbol of the core.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_NPM1300_DEFINE(sym, _bus, _addr)                                         \
  PBL_DEVICE_STATE_DEFINE(sym);                                                      \
  static struct pbl_npm1300_state sym##_npm1300_state;                               \
  const struct pbl_npm1300 sym = {                                                   \
    .dev = PBL_DEVICE_INIT(sym, "npm1300", pbl_npm1300_init, &(_bus)->dev, nullptr), \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                 \
    .state = &sym##_npm1300_state,                                                   \
  };                                                                                 \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/**
 * @brief Get the core a cell belongs to.
 *
 * @param cell Cell device, a child of the core.
 * @return Core.
 */
static inline const struct pbl_npm1300 *pbl_npm1300_from_cell(const struct pbl_device *cell) {
  return container_of(cell->parent, const struct pbl_npm1300, dev);
}

/**
 * @brief Lock the registers, around a read-modify-write sequence.
 *
 * Single accesses lock on their own.
 *
 * @param pmic Core.
 */
void pbl_npm1300_lock(const struct pbl_npm1300 *pmic);

/**
 * @brief Unlock the registers.
 *
 * @param pmic Core.
 */
void pbl_npm1300_unlock(const struct pbl_npm1300 *pmic);

/**
 * @brief Read a register.
 *
 * @param pmic Core.
 * @param reg Register address.
 * @param[out] val Register value.
 * @return 0 on success, or a negative errno.
 */
int pbl_npm1300_read(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t *val);

/**
 * @brief Write a register.
 *
 * @param pmic Core.
 * @param reg Register address.
 * @param val Value.
 * @return 0 on success, or a negative errno.
 */
int pbl_npm1300_write(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t val);

/** @} */
