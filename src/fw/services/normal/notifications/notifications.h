/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/item.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  ActionResultTypeSuccess,
  ActionResultTypeFailure,
  ActionResultTypeChaining,
  ActionResultTypeDoResponse,
  ActionResultTypeSuccessANCSDismiss,
} ActionResultType;

typedef struct {
  Uuid id;
  ActionResultType type;
  AttributeList attr_list;
  TimelineItemActionGroup action_group;
} PebbleSysNotificationActionResult;


void notifications_init(void);

//! Feedback for the result of an invoke action command
void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result);

//! Add a notification.
void notifications_handle_notification_added(Uuid *notification_id);

//! Handle a notification getting acted upon on the phone
void notifications_handle_notification_acted_upon(Uuid *notification_id);

//! Remove a notification
void notifications_handle_notification_removed(Uuid *notification_id);

//! Notify of remove command from ANCS. Notification will be kept in history
void notifications_handle_ancs_notification_removed(uint32_t ancs_uid);

//! Migration hook for notifications
//! Called with the GMT offset of the new timezone
void notifications_migrate_timezone(const int new_tz_offset);

//! Inserts a new notification into notification storage and notifies the system of the new item
//! @param notification Pointer to the notification to add
void notifications_add_notification(TimelineItem *notification);
