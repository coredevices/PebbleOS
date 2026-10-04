/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

//! @file
//!
//! Button lock: the "Lock Buttons" Quick Launch action locks all button and
//! touch input; holding the gesture bound to that action again for
//! shell_prefs_get_button_lock_hold_ms unlocks. Only hold and combo Quick
//! Launch gestures count, a single tap is too easy to trigger by accident.
//!
//! The locked state is intentionally RAM-only: a reboot always unlocks. The
//! hardware reset combo is handled at ISR level in the button driver and is
//! unaffected by the lock.
//!
//! The lock can also engage on its own after shell_prefs_get_button_lock_auto_ms
//! of inactivity. Auto-lock requires an unlock gesture, since without one the
//! lock could never be released, and it only ever arms after the first
//! activity following boot, so a fresh boot is always usable.

//! Create resources used by the button lock. Called from shell_event_loop_init.
void button_lock_init(void);

bool button_lock_is_locked(void);

//! Engage the lock. Does nothing without an unlock gesture, since the lock
//! could never be released. Must run on KernelMain.
void button_lock_engage(void);

//! Whether some gesture is configured that releases the lock.
bool button_lock_has_unlock_gesture(void);

//! Feed every button event on KernelMain before any other handling.
//! @return true if the event must be swallowed (masked from all tasks).
bool button_lock_handle_button_event(PebbleEvent *e);

//! Feed non-button user activity (e.g. touch) on KernelMain so it postpones
//! auto-locking, just like a button press does. Only feed deliberate contact:
//! a stream of false touches must not keep the watch from locking.
void button_lock_handle_activity(void);

//! Auto-lock never engages while the charger is plugged in, so arm or disarm
//! the idle timer when the cable comes and goes instead of polling for it.
void button_lock_handle_charger_change(bool is_plugged);

//! Re-evaluate the lock after its prefs or the gestures that release it
//! changed, and release it if no unlock gesture is left.
void button_lock_handle_prefs_changed(void);

#if UNITTEST
void button_lock_disarm_auto_lock_for_test(void);
#endif
