/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>
#include <stddef.h>

#include <pbl/util/size.h>

/**
 * @defgroup drivers_battery Battery
 * @ingroup drivers
 * @brief The battery cell, as the hardware description gives it.
 * @{
 */

/** @brief A point of an open-circuit voltage curve. */
struct pbl_battery_ocv_point {
  /** Open-circuit voltage in uV. */
  uint32_t uv;
  /** State of charge in percent. */
  uint8_t pct;
};

/**
 * @brief The battery cell, as the hardware description gives it.
 *
 * Data, not a device: chargers and fuel gauges read it. Fields the description lacks are 0.
 */
struct pbl_battery {
  /** Design capacity in uAh. */
  uint32_t charge_full_design_uah;
  /** Internal resistance in uOhm. */
  uint32_t internal_resistance_uohm;
  /** Constant charge current in uA. */
  uint32_t constant_charge_current_ua;
  /** Charge termination current in uA. */
  uint32_t charge_term_current_ua;
  /** Constant charge voltage in uV. */
  uint32_t constant_charge_voltage_uv;
  /** Open-circuit voltage curve, from full to empty, or NULL. */
  const struct pbl_battery_ocv_point *ocv;
  /** Number of points of @c ocv. */
  size_t ocv_count;
};

/** @brief The board's battery. */
extern const struct pbl_battery *const BATTERY;

/**
 * @brief Define a battery.
 *
 * @param sym Symbol of the battery.
 * @param _capacity_uah Design capacity in uAh.
 * @param _resistance_uohm Internal resistance in uOhm.
 * @param _chg_current_ua Constant charge current in uA.
 * @param _term_current_ua Charge termination current in uA.
 * @param _chg_voltage_uv Constant charge voltage in uV.
 * @param ... Open-circuit voltage curve, as {uV, percent} points from full to empty, if any.
 */
#define PBL_BATTERY_DEFINE(sym, _capacity_uah, _resistance_uohm, _chg_current_ua,    \
                           _term_current_ua, _chg_voltage_uv, ...)                   \
  __VA_OPT__(static const struct pbl_battery_ocv_point sym##_ocv[] = {__VA_ARGS__};) \
  const struct pbl_battery sym = {                                                   \
    .charge_full_design_uah = (_capacity_uah),                                       \
    .internal_resistance_uohm = (_resistance_uohm),                                  \
    .constant_charge_current_ua = (_chg_current_ua),                                 \
    .charge_term_current_ua = (_term_current_ua),                                    \
    .constant_charge_voltage_uv = (_chg_voltage_uv),                                 \
    __VA_OPT__(.ocv = sym##_ocv, .ocv_count = PBL_ARRAY_SIZE(sym##_ocv), )           \
  }

/** @} */
