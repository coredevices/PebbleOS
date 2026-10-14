/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/notifications/notifications.h>

PBL_WEAK void notifications_init(void) {
}

PBL_WEAK void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result) {
}

PBL_WEAK void notifications_handle_notification_added(Uuid *notification_id) {
}

PBL_WEAK void notifications_handle_notification_acted_upon(Uuid *notification_id) {
}

PBL_WEAK void notifications_handle_notification_removed(Uuid *notification_id) {
}

PBL_WEAK void notifications_migrate_timezone(const int new_tz_offset) {
}

PBL_WEAK void notifications_add_notification(TimelineItem *notification) {
}
