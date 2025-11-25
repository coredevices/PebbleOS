/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Initialize the PMIC driver. Call this once at startup.
bool pmic_init(void);

//! Tell the PMIC to power off the board and enter a standby-like state. All components will
//! have their power removed (except for the RTC so we'll still keep time) and the PMIC itself
//! will monitor the buttons for when to wake up.
bool pmic_power_off(void);

//! Enable the battery monitor portion of the PMIC. Remember to turn this off with
//! pmic_disable_battery_measure when immediate readings aren't required.
bool pmic_enable_battery_measure(void);

//! Disable the battery monitor portion of the PMIC.
bool pmic_disable_battery_measure(void);

//! Enable and disable the charging portion of the PMIC.
bool pmic_set_charger_state(bool enable);

//! @return true if the PMIC thinks we're charging (adding additional charge to the battery).
//! Note that once we hit full charge we'll no longer be charging, which is a different state
//! that pmic_is_usb_connected.
bool pmic_is_charging(void);

//! @return true if a usb-ish charger cable is currently connected.
bool pmic_is_usb_connected(void);

//! Read information about the chip for tracking purposes.
void pmic_read_chip_info(uint8_t *chip_id, uint8_t *chip_revision);

// FIXME: The following functions are unrelated to the PMIC and should be moved to the
// display/accessory connector drivers once we have them.

//! Enables the LDO3 power rail. Used for the MFi/Magnetometer on snowy_bb, MFi on snowy_evt.
void set_ldo3_power_state(bool enabled);

//! Enables the 4.5V power rail. Used for the display on snowy.
void set_4V5_power_state(bool enabled);

//! Enables the 6.6V power rail. Used for the display on snowy.
void set_6V6_power_state(bool enabled);

//! Enables power to the accessory connector.
void set_accessory_power_state(bool enabled);
