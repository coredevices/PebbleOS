/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Samples the ambient light for Ambient dark mode outside of the rendering path.
//! Must be called after the shell prefs are initialized.
void dark_mode_updater_init(void);

//! Starts or stops the sampling to match the dark mode pref. Call when that pref changes.
//! Does nothing until \ref dark_mode_updater_init has run.
void dark_mode_updater_prefs_changed(void);
