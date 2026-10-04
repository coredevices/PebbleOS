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

//! Re-evaluate the lock after the gestures that release it changed, and
//! release it if none is left.
void button_lock_handle_prefs_changed(void);
