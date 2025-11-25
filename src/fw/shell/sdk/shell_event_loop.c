/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_app_ids.h"
#include "apps/system_apps/timeline/timeline.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "popups/notifications/notification_window.h"
#include "popups/timeline/peek.h"
#include "process_management/app_manager.h"
#include "services/common/compositor/compositor.h"
#include "services/normal/activity/activity.h"
#include "services/normal/app_inbox_service.h"
#include "services/normal/app_outbox_service.h"
#include "shell/prefs.h"
#include "shell/sdk/watchface.h"
#include "shell/shell_event_loop.h"

extern void shell_prefs_init(void);

void shell_event_loop_init(void) {
  shell_prefs_init();
  notification_window_service_init();
  app_inbox_service_init();
  app_outbox_service_init();
  app_message_sender_init();
  watchface_init();
  timeline_peek_init();
#if CAPABILITY_HAS_HEALTH_TRACKING
  // Start activity tracking if enabled
  if (activity_prefs_tracking_is_enabled()) {
    activity_start_tracking(false /*test_mode*/);
  }
#endif
}

void shell_event_loop_handle_event(PebbleEvent *e) {
  switch (e->type) {
    case PEBBLE_APP_FETCH_REQUEST_EVENT:
      app_manager_handle_app_fetch_request_event(&e->app_fetch_request);
      return;

    case PEBBLE_SYS_NOTIFICATION_EVENT:
      notification_window_handle_notification(&e->sys_notification);
      return;

    case PEBBLE_REMINDER_EVENT:
      // This handles incoming Reminders
      notification_window_handle_reminder(&e->reminder);
      return;

    default:
      break; // don't care
  }
}
