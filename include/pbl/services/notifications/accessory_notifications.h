/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/item.h"

#include <stdbool.h>

//! Register the AccessoryNotifications consumer with the BT transport. Called once
//! from notifications_init().
void accessory_notifications_init(void);

//! Invoked from timeline_invoke_action when the user selects a forwarded
//! notification's action. Seals {notificationId, actionId, replyText} back to the
//! phone over the transport (AccessoryToHost). `reply_attributes` carries the
//! chosen/dictated text under AttributeIdTitle for a text-input action, or is NULL
//! for a plain action. Returns true once the reply is sealed onto the transport.
bool accessory_notifications_invoke_action(const TimelineItem *item,
                                           const TimelineItemAction *action,
                                           const AttributeList *reply_attributes);
