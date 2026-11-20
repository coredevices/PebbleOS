/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @brief Start reporting external power changes as battery connection events.
 */
void battery_charger_init(void);

/**
 * @brief Check whether USB power is connected.
 *
 * Always false while charging is forced off with battery_force_charge_enable().
 *
 * @return True if USB power is present.
 */
bool battery_is_usb_connected(void);

/**
 * @brief Check whether the charger reports charging.
 *
 * This is the raw charger status, not the notion of charging used by the rest of the system,
 * which comes from the battery state service. Always false while charging is forced off with
 * battery_force_charge_enable().
 *
 * @return True if the charger is charging.
 */
bool battery_charge_controller_thinks_we_are_charging(void);

/**
 * @brief Force charging on or off.
 *
 * While forced off, battery_is_usb_connected() and
 * battery_charge_controller_thinks_we_are_charging() report false.
 *
 * @param is_charging True to allow charging, false to force it off.
 */
void battery_force_charge_enable(bool is_charging);
