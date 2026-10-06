/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/accessory_notifications.h"

// Stub for tests that link services/timeline/timeline.c, which calls
// accessory_notifications_invoke_action() when a forwarded notification's action
// is selected. These tests don't exercise the BT transport, so the reply is never
// sealed: return false. Timeline surfaces that as a "Failed" action result, which
// is fine for tests that don't drive the reply path.

void accessory_notifications_init(void) {
}

bool accessory_notifications_invoke_action(const TimelineItem *item,
                                           const TimelineItemAction *action,
                                           const AttributeList *reply_attributes) {
  return false;
}
