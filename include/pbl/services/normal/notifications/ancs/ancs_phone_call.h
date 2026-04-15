/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"

//! Puts an incoming call event
//! @param uid ANCS UID of the incoming call notification
//! @param properties ANCS properties provided by the ANCS client
//! @param notif_attributes The notification attributes containing things such as caller id
void ancs_phone_call_handle_incoming(uint32_t uid, ANCSProperty properties,
                                     ANCSAttribute **notif_attributes);

//! Puts a hide call event - used in response to an ANCS removal message
//! @param uid ANCS UID of the removed incoming call notification
//! @param ios_9 Whether or not this notification was from an iOS 9 device
void ancs_phone_call_handle_removed(uint32_t uid, bool ios_9);

//! Returns true if we're currently ignoring missed calls (to avoid unnecessary notifications after
//! declining a call)
bool ancs_phone_call_should_ignore_missed_calls(void);

//! Blocks missed calls for a predetermined amount of time (called when dismissing a call from
//! the phone UI)
void ancs_phone_call_temporarily_block_missed_calls(void);
