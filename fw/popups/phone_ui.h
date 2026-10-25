/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/phone_call_util.h>

#include <kernel/events.h>

void phone_ui_handle_incoming_call(PebblePhoneCaller *caller, bool show_ongoing_call_ui,
                                   PhoneCallSource source);

void phone_ui_handle_outgoing_call(PebblePhoneCaller *caller);

void phone_ui_handle_missed_call(void);

void phone_ui_handle_call_start(bool can_decline);

void phone_ui_handle_call_end(bool call_accepted, bool disconnected);

void phone_ui_handle_call_hide(void);

void phone_ui_handle_caller_id(PebblePhoneCaller *caller);

#if defined(CONFIG_BT_ZEPHYR_CLASSIC) && defined(CONFIG_BT_CALL_AUDIO)
#include <time.h>

//! The screen of a call over the watch's link (dialed on the watch, or back to it), as the
//! ongoing call's: @p active_since 0 while it is not answered. On KernelMain.
void phone_ui_show_watch_call(const char *number, const char *name, time_t active_since);

//! That call was answered (@p active_since) or ended: its screen follows.
void phone_ui_watch_call_changed(time_t active_since, bool ended);

//! Whether the phone call screen shows.
bool phone_ui_is_visible(void);
#endif
