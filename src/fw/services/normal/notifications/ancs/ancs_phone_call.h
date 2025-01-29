/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
