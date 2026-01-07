/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Prefs Sync Integration
//!
//! This module integrates settings sync with the shell prefs system.
//! It handles:
//! - Whitelisting of syncable preferences
//! - Automatic sync on connection to phone
//! - Debouncing for rapid preference changes

//! Initialize prefs sync
//! Call this from shell_prefs_init() after prefs are loaded
void prefs_sync_init(void);

//! Deinitialize prefs sync
void prefs_sync_deinit(void);

//! Manually trigger a sync (e.g., for testing)
void prefs_sync_trigger(void);
