/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/battery.h>
#include <pbl/drivers/charger.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_charger_npm1300 nPM1300 charger
 * @ingroup drivers_charger
 * @brief @ref drivers_charger driver for the charger of the nPM1300 PMIC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_charger_npm1300_config {
  // Discharge current limit in mA, 200 or 1000
  uint16_t dischg_limit_ma;
  // Charge termination voltage in the warm and cool regions in mV
  uint16_t vterm_reduced_mv;
  uint16_t thermistor_beta;
  // VBUS current limits in mA with USB connected and at startup, 0 for the default
  uint16_t vbus_current_lim0;
  uint16_t vbus_current_startup;
  // NTC thermistor nominal resistance in kOhm: 10, 47 or 100
  uint8_t ntc_kohm;
  // Charging stops above it
  uint8_t ntc_hot_celsius;
};

struct pbl_charger_npm1300_data {
  const struct pbl_charger_npm1300 *chg;
  uint32_t dischg_limit_ma;
  struct pbl_delayed_work debounce_timer;
};

struct pbl_charger_npm1300 {
  struct pbl_charger charger;
  struct pbl_charger_npm1300_config cfg;
  // Charge current 32 to 800 mA in 2 mA steps, termination current 10 or 20% of it
  const struct pbl_battery *battery;
  // From the PMIC's GPIO1
  struct pbl_irq_spec irq;
  struct pbl_charger_npm1300_data *data;
};

extern const struct pbl_charger_ops pbl_charger_npm1300_ops;
/** @endcond */

/**
 * @brief Define the charger of an nPM1300.
 *
 * @param sym Symbol of the charger.
 * @param _pmic PMIC, a struct pbl_npm1300.
 * @param _irq Interrupt of the PMIC, from its GPIO1, a struct pbl_irq_spec.
 * @param _battery Battery, a struct pbl_battery.
 * @param _vterm_reduced_uv Charge termination voltage in the warm and cool regions, in uV.
 * @param _ntc_ohms Nominal resistance of the NTC thermistor, in ohms: 10000, 47000 or 100000.
 * @param _beta Beta of the NTC thermistor.
 * @param _hot_celsius Temperature above which charging stops, in Celsius.
 * @param _dischg_ua Discharge current limit, in uA: 200000 or 1000000.
 * @param _vbus_ua VBUS current limit with USB connected, in uA, 0 for the default.
 * @param _vbus_startup_ua VBUS current limit at startup, in uA, 0 for the default.
 */
#define PBL_CHARGER_NPM1300_DEFINE(sym, _pmic, _irq, _battery, _vterm_reduced_uv, _ntc_ohms,      \
                                   _beta, _hot_celsius, _dischg_ua, _vbus_ua, _vbus_startup_ua)   \
  PBL_CHARGER_STATE_DEFINE(sym);                                                                  \
  static struct pbl_charger_npm1300_data sym##_data;                                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_charger_npm1300, struct pbl_charger, charger) {             \
    .charger = PBL_CHARGER_INIT(sym, "npm1300_charger", &(_pmic)->dev, &pbl_charger_npm1300_ops), \
    .cfg =                                                                                        \
        {                                                                                         \
          .dischg_limit_ma = (_dischg_ua) / 1000,                                                 \
          .vterm_reduced_mv = (_vterm_reduced_uv) / 1000,                                         \
          .thermistor_beta = (_beta),                                                             \
          .vbus_current_lim0 = (_vbus_ua) / 1000,                                                 \
          .vbus_current_startup = (_vbus_startup_ua) / 1000,                                      \
          .ntc_kohm = (_ntc_ohms) / 1000,                                                         \
          .ntc_hot_celsius = (_hot_celsius),                                                      \
        },                                                                                        \
    .battery = (_battery), .irq = _irq, .data = &sym##_data,                                      \
  }

/** @} */
