/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>
#include <stdbool.h>

/**
 * @defgroup drivers_battery Battery
 * @ingroup drivers
 * @brief Battery and charger driver interface.
 *
 * Implemented by the PMIC driver. The battery state service builds state of charge and
 * charging state on top of it.
 * @{
 */

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

/** @brief Initialize the battery driver. */
void battery_init(void);

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

/** @} */
