/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

//! @file
//!
//! Button lock: holding Back+Down for a configurable duration
//! (shell_prefs_get_button_lock_hold_ms, 0 = disabled) locks all button and
//! touch input; holding the combo again unlocks.
//!
//! The locked state is intentionally RAM-only: a reboot always unlocks. The
//! hardware reset combo is handled at ISR level in the button driver and is
//! unaffected by the lock.
//!
//! The lock can also engage on its own after shell_prefs_get_button_lock_auto_ms
//! of inactivity. Auto-lock requires a configured hold duration, since without
//! the combo the lock could never be released, and it only ever arms after the
//! first activity following boot, so a fresh boot is always usable.

//! Create resources used by the button lock. Called from shell_event_loop_init.
void button_lock_init(void);

bool button_lock_is_locked(void);

//! Feed every button event on KernelMain before any other handling.
//! @return true if the event must be swallowed (masked from all tasks).
bool button_lock_handle_button_event(PebbleEvent *e);

//! Feed non-button user activity (e.g. touch) on KernelMain so it postpones
//! auto-locking, just like a button press does.
void button_lock_handle_activity(void);

//! Auto-lock never engages while the charger is plugged in, so arm or disarm
//! the idle timer when the cable comes and goes instead of polling for it.
void button_lock_handle_charger_change(bool is_plugged);

//! Re-evaluate the auto-lock state after the button lock prefs changed, and
//! release the lock if the unlock combo was just turned off.
void button_lock_handle_prefs_changed(void);

#if UNITTEST
void button_lock_disarm_auto_lock_for_test(void);
#endif
