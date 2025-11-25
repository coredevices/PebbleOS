/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "services/normal/phone_call_util.h"

#include <stdbool.h>

void phone_ui_handle_incoming_call(PebblePhoneCaller *caller, bool can_answer,
                                   bool show_ongoing_call_ui, PhoneCallSource source);

void phone_ui_handle_outgoing_call(PebblePhoneCaller *caller);

void phone_ui_handle_missed_call(void);

void phone_ui_handle_call_start(bool can_decline);

void phone_ui_handle_call_end(bool call_accepted, bool disconnected);

void phone_ui_handle_call_hide(void);

void phone_ui_handle_caller_id(PebblePhoneCaller *caller);
