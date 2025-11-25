/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Initialize the PMIC driver. Call this once at startup.
bool pmic_init(void);

//! Whether or not the usb charger is detected by the pmic.
bool pmic_is_usb_connected(void);

//! Returns true if this boot was caused by a charger event from standby, but
//! there is no charger connected.
bool pmic_boot_due_to_charger_disconnect(void);

//! Enter what we consider to be a "powered off" state, which is the as3701
//! standby state, where we keep the RTC powered.
bool pmic_power_off(void);

//! Fully shut down the as3701, which does not power the RTC.
bool pmic_full_power_off(void);
