/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>
#include <stddef.h>

#include <pbl/util/size.h>

/**
 * @defgroup drivers_battery Battery
 * @ingroup drivers
 * @brief Battery and charger driver interface.
 *
 * Implemented by the PMIC driver. The battery state service builds state of charge and
 * charging state on top of it.
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
    __VA_OPT__(.ocv = sym##_ocv, .ocv_count = ARRAY_LENGTH(sym##_ocv), )             \
  }

/** @brief Battery charge status. */
typedef enum {
  /** Unknown charge status. */
  BatteryChargeStatusUnknown,
  /** Charging is complete, battery full. */
  BatteryChargeStatusComplete,
  /** Trickle charging. */
  BatteryChargeStatusTrickle,
  /** Constant current charging. */
  BatteryChargeStatusCC,
  /** Constant voltage charging. */
  BatteryChargeStatusCV,
} BatteryChargeStatus;

/** @brief Battery measurements. */
typedef struct BatteryConstants {
  /** Battery voltage in millivolts. */
  int32_t v_mv;
  /** Battery current in microamperes. */
  int32_t i_ua;
  /** Battery temperature in millidegrees Celsius. */
  int32_t t_mc;
} BatteryConstants;

/**
 * @brief Handler of USB power connection changes.
 *
 * Called from a system work queue.
 *
 * @param is_connected What battery_is_usb_connected() reports.
 */
typedef void (*battery_connection_handler_t)(bool is_connected);

/** @brief Initialize the battery driver. */
void battery_init(void);

/**
 * @brief Set the handler called when USB power is connected or removed.
 *
 * Changes are not reported while there is no handler.
 *
 * @param handler Handler, or NULL for none.
 */
void battery_set_connection_handler(battery_connection_handler_t handler);

/**
 * @brief Report a USB power connection change to the handler.
 *
 * For drivers, from thread context.
 */
void battery_connection_changed(void);

/**
 * @brief Measure the battery voltage.
 *
 * @return Battery voltage in millivolts, 0 on failure.
 */
int battery_get_millivolts(void);

/**
 * @brief Measure battery voltage, current and temperature.
 *
 * @param[out] constants Measurements.
 * @retval 0 Success.
 * @retval negative Error code on failure.
 */
int battery_get_constants(BatteryConstants *constants);

/**
 * @brief Check whether the charge controller reports charging.
 *
 * This is the raw charger status, not the notion of charging used by the rest of the system,
 * which comes from the battery state service. Always false while charging is forced off with
 * battery_force_charge_enable().
 *
 * @return True if the charge controller is charging.
 */
bool battery_charge_controller_thinks_we_are_charging(void);

/**
 * @brief Check whether USB power is connected.
 *
 * Always false while charging is forced off with battery_force_charge_enable().
 *
 * @return True if USB power is present.
 */
bool battery_is_usb_connected(void);

/**
 * @brief Enable or disable the charger.
 *
 * @param charging_enabled True to allow charging.
 */
void battery_set_charge_enable(bool charging_enabled);

/**
 * @brief Enable or disable fast charging.
 *
 * A no-op on chargers that manage the charge current themselves.
 *
 * @param fast_charge_enabled True to allow fast charging.
 */
void battery_set_fast_charge(bool fast_charge_enabled);

/**
 * @brief Driver implementation of battery_is_usb_connected().
 *
 * Ignores battery_force_charge_enable().
 *
 * @return True if USB power is present.
 */
bool battery_is_usb_connected_impl(void);

/**
 * @brief Driver implementation of battery_charge_controller_thinks_we_are_charging().
 *
 * Ignores battery_force_charge_enable().
 *
 * @return True if the charge controller is charging.
 */
bool battery_charge_controller_thinks_we_are_charging_impl(void);

/**
 * @brief Force charging on or off.
 *
 * Enables or disables the charger. While forced off, battery_is_usb_connected() and
 * battery_charge_controller_thinks_we_are_charging() report false.
 *
 * @param is_charging True to allow charging, false to force it off.
 */
void battery_force_charge_enable(bool is_charging);

/**
 * @brief Get the charge status.
 *
 * @param[out] status Current charge status.
 * @retval 0 Success.
 * @retval negative Error code on failure.
 */
int battery_charge_status_get(BatteryChargeStatus *status);

/**
 * @brief Request a high discharge current, e.g. for the speaker.
 *
 * Requests nest: the battery may supply a high current until each request is released. Provided
 * by drivers that select @c BATTERY_HIGH_LOAD.
 *
 * @return 0 or a negative errno.
 */
int battery_high_load_request(void);

/**
 * @brief Release a request made with battery_high_load_request().
 *
 * @return 0 or a negative errno.
 */
int battery_high_load_release(void);

/** @} */
