/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>


void pp_answer_call(uint32_t cookie);

void pp_decline_call(uint32_t cookie);

void pp_get_phone_state(void);

//! Enables or disables handling the Get Phone State responses.
//! This is part of a work-around to ignore for stray requests that can be in flight after the phone
//! call has been declined by the user from the Pebble.
void pp_get_phone_state_set_enabled(bool enabled);
