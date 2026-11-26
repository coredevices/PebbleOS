/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

void timer_popup_push_window(const PebbleTimerEvent *event);

//! Whether the timer popup is currently shown.
bool timer_popup_is_active(void);

//! Dismiss the timer popup without showing it as acknowledged, keeping its
//! duration queued for later. Used when an alarm takes over the alert.
void timer_popup_abort(void);

//! Show the oldest queued expiry, if the alert resources are free.
void timer_popup_push_pending(void);
