/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/battery/battery_monitor.h>

#include <applib/graphics/gtypes.h>

typedef enum BatteryUIWarningLevel {
  BatteryUIWarningLevel_None = -1,
  BatteryUIWarningLevel_Low,
  BatteryUIWarningLevel_VeryLow
} BatteryUIWarningLevel;

//! Process the incoming battery state change notification
void battery_ui_handle_state_change_event(PreciseBatteryChargeState new_state);

//! Handle shutting down the watch.
//!
//! @retval -EBUSY The watch can't shut down while it is plugged in.
int battery_ui_handle_shut_down(void);

//! Show the 'battery charging' modal dialog
void battery_ui_display_plugged(uint8_t percent);

//! Update the charge level shown by the 'battery charging' modal dialog
void battery_ui_update_charge_level(uint8_t percent);

//! Show the 'battery charged' modal dialog
void battery_ui_display_fully_charged(void);

//! Show the 'battery critical' modal dialog
void battery_ui_display_warning(uint32_t percent, BatteryUIWarningLevel warning_level);

//! Dismiss the battery UI modal window.
void battery_ui_dismiss_modal(void);
