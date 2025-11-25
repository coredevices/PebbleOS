/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/notifications.h"

void notifications_init(void) {}

void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result) {}

void notifications_handle_notification_added(Uuid *notification_id) {}

void notifications_handle_notification_acted_upon(Uuid *notification_id) {}

void notifications_handle_notification_removed(Uuid *notification_id) {}

void notifications_handle_ancs_notification_removed(uint32_t ancs_uid) {}

void notifications_migrate_timezone(const int new_tz_offset) {}

void notifications_add_notification(TimelineItem *notification) {}
