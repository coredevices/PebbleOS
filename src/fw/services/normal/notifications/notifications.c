/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notifications.h"

#include "notification_storage.h"
#include "do_not_disturb.h"

#include "applib/ui/vibes.h"
#include "drivers/rtc.h"
#include "drivers/battery.h"
#include "resource/resource_ids.auto.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/bitset.h"

#include "kernel/events.h"
#include "kernel/low_power.h"
#include "kernel/pbl_malloc.h"

#include "services/common/analytics/analytics.h"
#include "services/common/evented_timer.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/blob_db/reminder_db.h"
#include "services/normal/phone_call.h"
#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/timeline.h"
#include "services/normal/vibes/vibe_intensity.h"

#include <string.h>

static void prv_notification_migration_iterator_callback(TimelineItem *notification,
    SerializedTimelineItemHeader *header, void *data) {
  header->common.timestamp -= *((int*)data);
  notification->header.timestamp = header->common.timestamp;
}

void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationActionResult,
      .action_result = action_result,
    }
  };
  // event loop will free memory of action_result
  event_put(&launcher_event);
}

void notifications_handle_notification_removed(Uuid *notification_id) {
  Uuid *removed_id = kernel_malloc_check(sizeof(Uuid));
  *removed_id = *notification_id;
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationRemoved,
      .notification_id = removed_id,
    }
  };
  event_put(&launcher_event);
}

void notifications_handle_notification_added(Uuid *notification_id) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationAdded,
      .notification_id = notification_id
    }
  };
  event_put(&launcher_event);
  analytics_inc(ANALYTICS_DEVICE_METRIC_NOTIFICATION_RECEIVED_COUNT, AnalyticsClient_System);
}

void notifications_handle_notification_acted_upon(Uuid *notification_id) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationActedUpon,
      .notification_id = notification_id
    }
  };
  event_put(&launcher_event);
}

void notifications_migrate_timezone(const int tz_diff) {
  notification_storage_rewrite(prv_notification_migration_iterator_callback, (void*)&tz_diff);
}

void notification_storage_init(void);
void vibe_intensity_init(void);

void notifications_init(void) {
  notification_storage_init();
}

void notifications_add_notification(TimelineItem *notification) {
  notification_storage_store(notification);

  Uuid *uuid = kernel_malloc_check(sizeof(Uuid));
  *uuid = notification->header.id;
  notifications_handle_notification_added(uuid);
}
