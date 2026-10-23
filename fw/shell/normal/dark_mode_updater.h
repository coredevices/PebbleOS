/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Keeps dark mode up to date outside of the rendering path: samples the ambient light for Ambient
//! mode and watches the clock for Scheduled mode. Puts a PEBBLE_DARK_MODE_CHANGE_EVENT whenever
//! the effective dark mode flips. Must be called after the shell prefs are initialized.
void dark_mode_updater_init(void);

//! Re-evaluates dark mode and restarts the checks to match the prefs. Call when the dark mode or
//! its schedule changes.
//! Does nothing until \ref dark_mode_updater_init has run.
void dark_mode_updater_prefs_changed(void);
