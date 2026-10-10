/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/regulator.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/new_timer/new_timer.h>

#include <board/board.h>

/**
 * @defgroup drivers_pmic_npm1300 nPM1300
 * @ingroup drivers_pmic
 * @brief Nordic nPM1300 PMIC, as a multi-function device.
 *
 * The PMIC is a struct pbl_npm1300 device on its I2C bus. It owns the register access, under a
 * lock so its functions cannot interleave read-modify-write sequences, and implements the
 * @ref drivers_pmic and battery APIs. Its other functions are child devices brought up by its
 * init: a @ref drivers_gpio port over its five GPIOs, and a @ref drivers_regulator per rail the
 * board defines.
 *
 * @code{.c}
 * PBL_NPM1300_DEFINE(s_npm1300, &s_i2c1.bus, 0x6B, &NPM1300_CONFIG,
 *                    {.peripheral = hwp_gpio1, .gpio_pin = 26});
 * PBL_NPM1300_REGULATOR_DEFINE(s_ldo2, "ldo2", &s_npm1300, PBL_NPM1300_LDSW2, 3300, true, false);
 *
 * static const struct pbl_gpio s_reset = PBL_GPIO(&s_npm1300.gpio, 2, PBL_GPIO_ACTIVE_LOW);
 * @endcode
 * @{
 */

/** @brief nPM1300 board configuration. */
typedef struct {
  /** Charge current in mA, 32 to 800 in 2 mA steps. */
  uint16_t chg_current_ma;
  /** Discharge current limit in mA, 200 or 1000. */
  uint16_t dischg_limit_ma;
  /** Charge termination current, in percent of the charge current: 10 or 20. */
  uint8_t term_current_pct;
  /** Charge termination voltage in mV: 3500 to 3650 or 4000 to 4450, in 50 mV steps. */
  uint16_t vterm_mv;
  /** Charge termination voltage in the warm and cool regions, same range. */
  uint16_t vterm_reduced_mv;
  /** NTC thermistor nominal resistance in kOhm: 10, 47 or 100. */
  uint8_t ntc_kohm;
  /** Thermistor beta value. */
  uint16_t thermistor_beta;
  /** NTC hot threshold in degrees Celsius; charging stops above it. */
  uint8_t ntc_hot_celsius;
  /** VBUS current limit in mA applied when USB is connected, 0 to keep the default. */
  uint16_t vbus_current_lim0;
  /** VBUS current limit in mA applied at startup, 0 to keep the default. */
  uint16_t vbus_current_startup;
} Npm1300Config;

/** @brief Maximum discharge current limit in mA. */
#define NPM1300_DISCHG_LIMIT_MA_MAX 1000UL

/** @cond INTERNAL_HIDDEN */
struct pbl_npm1300_state {
  struct pbl_mutex lock;
  uint32_t dischg_limit_ma;
  TimerID debounce_charger_timer;
  uint8_t gpio_pullup_mask;
};
/** @endcond */

/** @brief nPM1300 PMIC. */
struct pbl_npm1300 {
  /** Device, child of the I2C bus. */
  struct pbl_device dev;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Interrupt line, from the PMIC's GPIO1. */
  ExtiConfig irq;
  /** Charger configuration. */
  const Npm1300Config *cfg;
  /** Runtime state. */
  struct pbl_npm1300_state *state;
  /** GPIO port child. */
  struct pbl_gpio_port gpio;
};

/** @brief The board's PMIC. */
extern const struct pbl_npm1300 *const NPM1300;

/** @brief The board's charger configuration. */
extern const Npm1300Config NPM1300_CONFIG;

/** @cond INTERNAL_HIDDEN */
int pbl_npm1300_init(const struct pbl_device *dev);
extern const struct pbl_gpio_port_ops pbl_npm1300_gpio_ops;
/** @endcond */

/**
 * @brief Define the PMIC and its GPIO port.
 *
 * The board also defines @c NPM1300 to point at it.
 *
 * @param sym Symbol of the PMIC.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _cfg Charger configuration.
 * @param ... Interrupt line, an @c ExtiConfig initializer.
 */
#define PBL_NPM1300_DEFINE(sym, _bus, _addr, _cfg, ...)                           \
  PBL_DEVICE_STATE_DEFINE(sym);                                                   \
  PBL_DEVICE_STATE_DEFINE(sym##_gpio);                                            \
  static struct pbl_npm1300_state sym##_npm1300_state;                            \
  const struct pbl_npm1300 sym = {                                                \
    .dev = PBL_DEVICE_INIT(sym, "npm1300", pbl_npm1300_init, &(_bus)->dev, NULL), \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                              \
    .irq = __VA_ARGS__,                                                           \
    .cfg = (_cfg),                                                                \
    .state = &sym##_npm1300_state,                                                \
    .gpio = {                                                                     \
      .dev = PBL_DEVICE_INIT(sym##_gpio, "npm1300_gpio", NULL, &sym.dev, NULL),   \
      .ops = &pbl_npm1300_gpio_ops,                                               \
    },                                                                            \
  };                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.dev);                                             \
  PBL_DEVICE_REGISTER(sym##_gpio, &sym.gpio.dev)

/**
 * @brief Lock the PMIC registers, around a read-modify-write sequence.
 *
 * Single accesses lock on their own.
 *
 * @param pmic PMIC.
 */
void pbl_npm1300_lock(const struct pbl_npm1300 *pmic);

/**
 * @brief Unlock the PMIC registers.
 *
 * @param pmic PMIC.
 */
void pbl_npm1300_unlock(const struct pbl_npm1300 *pmic);

/**
 * @brief Read a register.
 *
 * @param pmic PMIC.
 * @param reg Register address.
 * @param[out] val Register value.
 * @return True on success.
 */
bool pbl_npm1300_read(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t *val);

/**
 * @brief Write a register.
 *
 * @param pmic PMIC.
 * @param reg Register address.
 * @param val Value.
 * @return True on success.
 */
bool pbl_npm1300_write(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t val);

/**
 * @brief Set the battery discharge current limit.
 *
 * @param pmic PMIC.
 * @param ma Limit in mA, 200 or 1000.
 * @return True on success.
 */
bool pbl_npm1300_set_dischg_limit_ma(const struct pbl_npm1300 *pmic, uint32_t ma);

/** @brief nPM1300 rails. */
enum pbl_npm1300_rail {
  /** BUCK1. */
  PBL_NPM1300_BUCK1,
  /** BUCK2. */
  PBL_NPM1300_BUCK2,
  /** Load switch / LDO 1. */
  PBL_NPM1300_LDSW1,
  /** Load switch / LDO 2. */
  PBL_NPM1300_LDSW2,
};

/** @brief nPM1300 rail. */
struct pbl_npm1300_regulator {
  /** Regulator, child of the PMIC. */
  struct pbl_regulator reg;
  /** Rail. */
  enum pbl_npm1300_rail rail;
  /** Output voltage in mV, 1000 to 3300 in 100 mV steps; unused by a load switch. */
  uint16_t voltage_mv;
  /** LDSW rails only: run as an LDO rather than a load switch. */
  bool ldo;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_regulator_ops pbl_npm1300_regulator_ops;
/** @endcond */

/**
 * @brief Define a PMIC rail.
 *
 * @param sym Symbol of the rail; the regulator is @c sym.reg.
 * @param _name Name.
 * @param _pmic PMIC, a struct pbl_npm1300.
 * @param _rail Rail, an @ref pbl_npm1300_rail.
 * @param _voltage_mv Output voltage in mV.
 * @param _ldo LDSW rails only: run as an LDO.
 * @param _always_on Switched on at init and never off.
 */
#define PBL_NPM1300_REGULATOR_DEFINE(sym, _name, _pmic, _rail, _voltage_mv, _ldo, _always_on)     \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                                \
  const struct pbl_npm1300_regulator sym = {                                                      \
    .reg = PBL_REGULATOR_INIT(sym, _name, &(_pmic)->dev, &pbl_npm1300_regulator_ops, _always_on), \
    .rail = (_rail),                                                                              \
    .voltage_mv = (_voltage_mv),                                                                  \
    .ldo = (_ldo),                                                                                \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.reg.dev)

/** @} */
