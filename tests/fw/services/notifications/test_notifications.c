/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Specs for the notification events service.
//
// The Notifications app and the launcher glance hide a notification once it is actioned. A
// successful action marks it here rather than in a window, since the window may have closed by
// the time the phone replies. The event still has to go out afterwards for the windows to update.

#include "clar.h"

#include "pbl/services/notifications/notifications.h"
#include "pbl/services/timeline/item.h"

#include "fake_events.h"
#include "fake_notification_storage_status.h"

#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_notification_storage.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

static const Uuid s_id = {0x21, 0x90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x09};

void test_notifications__initialize(void) {
  fake_event_init();
  fake_notification_storage_status_reset();
}

void test_notifications__cleanup(void) {
}

// An action that worked hides the notification, and the windows still hear about it.
void test_notifications__successful_action_marks_actioned(void) {
  PebbleSysNotificationActionResult action_result = {
    .id = s_id,
    .type = ActionResultTypeSuccess,
  };

  notifications_handle_notification_action_result(&action_result);

  Uuid status_id;
  const uint8_t result = fake_notification_storage_status_get_last(&status_id);
  cl_assert(uuid_equal(&status_id, &s_id));
  cl_assert_equal_i(result, TimelineItemStatusActioned);
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(fake_event_get_last().sys_notification.type, NotificationActionResult);
}

// The iPhone's dismiss reply is a success too, so it hides the notification the same way.
void test_notifications__ancs_dismiss_marks_actioned(void) {
  PebbleSysNotificationActionResult action_result = {
    .id = s_id,
    .type = ActionResultTypeSuccessANCSDismiss,
  };

  notifications_handle_notification_action_result(&action_result);

  Uuid status_id;
  const uint8_t result = fake_notification_storage_status_get_last(&status_id);
  cl_assert(uuid_equal(&status_id, &s_id));
  cl_assert_equal_i(result, TimelineItemStatusActioned);
}

// A failed action leaves the notification in the history, so the wearer can try again.
void test_notifications__failed_action_leaves_storage_alone(void) {
  PebbleSysNotificationActionResult action_result = {
    .id = s_id,
    .type = ActionResultTypeFailure,
  };

  notifications_handle_notification_action_result(&action_result);

  cl_assert(!fake_notification_storage_status_was_set());
}

// Dialling from an iPhone notification sends no result. That must not crash, and the windows
// still get the event so they can close the action menu.
void test_notifications__missing_action_result_leaves_storage_alone(void) {
  notifications_handle_notification_action_result(NULL);

  cl_assert(!fake_notification_storage_status_was_set());
  cl_assert_equal_i(fake_event_get_count(), 1);
}
