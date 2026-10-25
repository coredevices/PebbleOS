/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// While a call over the watch's link goes on and its screen does not show, a bar across the
// top of the screen says so; a tap on it, or Select and Down together, brings the call's
// screen (the phone call screen) back. On KernelMain but where said.

#include <stdbool.h>

#include <pbl/services/touch/touch_event.h>

#include <kernel/events.h>

//! The call's state or what is on screen may have changed: show or hide the bar.
void call_indicator_update(void);

//! A touch, before it goes anywhere else: true when the bar took it.
bool call_indicator_handle_touch(const TouchEvent *event);

//! Whether the bar shows at (@p x, @p y): its gestures are not the app's.
bool call_indicator_covers(int16_t x, int16_t y);

//! A button event, before it goes anywhere else: true when it is part of Select and Down
//! together during a call, which the app does not get.
bool call_indicator_handle_button(const PebbleEvent *event);

//! The call's screen, for a call just dialed to @p number (any task).
void call_indicator_open_call_screen(const char *number);
