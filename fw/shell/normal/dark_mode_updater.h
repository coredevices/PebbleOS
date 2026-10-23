/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <shell/prefs.h>

//! Keeps the effective dark mode up to date, following the clock in Scheduled mode. Puts a
//! PEBBLE_DARK_MODE_CHANGE_EVENT whenever it flips. Must be called after the shell prefs are
//! initialized.
void dark_mode_updater_init(void);

//! Re-evaluates dark mode and starts or stops following the clock to match the prefs. Call when
//! the dark mode or its schedule changes.
void dark_mode_updater_prefs_changed(void);

//! @return true if \a minute_of_day (0 to 1439) falls within \a schedule. The window starts at the
//! "from" time, ends before the "to" time and can wrap past midnight; it is empty when both match.
bool dark_mode_schedule_is_active(const DarkModeSchedule *schedule, int minute_of_day);
