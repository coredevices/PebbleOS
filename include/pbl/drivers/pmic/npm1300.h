/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/battery.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_pmic_npm1300 nPM1300 charger
 * @ingroup drivers_pmic
 * @brief Charger of the nPM1300, a cell of @ref drivers_mfd_npm1300.
 *
 * Implements the @ref drivers_pmic and battery APIs, and owns the PMIC interrupt line, which
 * reports VBUS and charge events.
 * @{
 */

/** @brief nPM1300 board configuration. */
typedef struct {
  /** Discharge current limit in mA, 200 or 1000. */
  uint16_t dischg_limit_ma;
  /** Charge termination voltage in the warm and cool regions in mV: 3500 to 3650 or 4000 to
   * 4450, in 50 mV steps. */
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
struct pbl_npm1300_charger_state {
  uint32_t dischg_limit_ma;
  uint32_t high_load_count;
  struct pbl_delayed_work debounce_timer;
};
/** @endcond */

/** @brief nPM1300 charger. */
struct pbl_npm1300_charger {
  /** Device, child of the core. */
  struct pbl_device dev;
  /** Charger configuration. */
  const Npm1300Config *cfg;
  /** The battery charged: charge current 32 to 800 mA in 2 mA steps, termination current 10 or
   * 20% of it, and termination voltage 3500 to 3650 or 4000 to 4450 mV, in 50 mV steps. */
  const struct pbl_battery *battery;
  /** Interrupt line, from the PMIC's GPIO1. */
  struct pbl_irq_spec irq;
  /** Runtime state. */
  struct pbl_npm1300_charger_state *state;
};

/** @brief The board's charger. */
extern const struct pbl_npm1300_charger *const NPM1300_CHARGER;

/** @cond INTERNAL_HIDDEN */
int pbl_npm1300_charger_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the charger.
 *
 * The board also defines @c NPM1300_CHARGER to point at it.
 *
 * @param sym Symbol of the charger.
 * @param _pmic Core, a struct pbl_npm1300.
 * @param _irq Interrupt line, a struct pbl_irq_spec initializer.
 * @param _battery The battery charged, a struct pbl_battery.
 * @param _vterm_reduced_uv Termination voltage in the warm and cool regions, in uV.
 * @param _ntc_ohms NTC thermistor nominal resistance, in ohm.
 * @param _beta Thermistor beta value.
 * @param _hot_celsius Charging stops above this temperature.
 * @param _dischg_ua Discharge current limit, in uA.
 * @param _vbus_ua VBUS current limit with USB connected, in uA, 0 to keep the default.
 * @param _vbus_startup_ua VBUS current limit at startup, in uA, 0 to keep the default.
 */
#define PBL_NPM1300_CHARGER_DEFINE(sym, _pmic, _irq, _battery, _vterm_reduced_uv, _ntc_ohms,       \
                                   _beta, _hot_celsius, _dischg_ua, _vbus_ua, _vbus_startup_ua)    \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  static struct pbl_npm1300_charger_state sym##_npm1300_charger_state;                             \
  static const Npm1300Config sym##_cfg = {                                                         \
    .dischg_limit_ma = (_dischg_ua) / 1000,                                                        \
    .vterm_reduced_mv = (_vterm_reduced_uv) / 1000,                                                \
    .ntc_kohm = (_ntc_ohms) / 1000,                                                                \
    .thermistor_beta = (_beta),                                                                    \
    .ntc_hot_celsius = (_hot_celsius),                                                             \
    .vbus_current_lim0 = (_vbus_ua) / 1000,                                                        \
    .vbus_current_startup = (_vbus_startup_ua) / 1000,                                             \
  };                                                                                               \
  const struct pbl_npm1300_charger sym = {                                                         \
    .dev =                                                                                         \
        PBL_DEVICE_INIT(sym, "npm1300_charger", pbl_npm1300_charger_init, &(_pmic)->dev, nullptr), \
    .cfg = &sym##_cfg,                                                                             \
    .battery = (_battery),                                                                         \
    .irq = _irq,                                                                                   \
    .state = &sym##_npm1300_charger_state,                                                         \
  };                                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/**
 * @brief Set the battery discharge current limit.
 *
 * The limit is @ref NPM1300_DISCHG_LIMIT_MA_MAX while a battery_high_load_request() is held.
 *
 * @param charger Charger.
 * @param ma Limit in mA, 200 or 1000.
 * @return 0 on success, or a negative errno.
 */
int pbl_npm1300_charger_set_dischg_limit_ma(const struct pbl_npm1300_charger *charger, uint32_t ma);

/** @} */
