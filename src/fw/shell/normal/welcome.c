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

#include "welcome.h"

#include "kernel/event_loop.h"
#include "resource/timeline_resource_ids.auto.h"
#include "services/common/evented_timer.h"
#include "services/common/i18n/i18n.h"
#include "services/common/shared_prf_storage/shared_prf_storage.h"
#include "shell/prefs.h"
#include "system/logging.h"
#include "util/attributes.h"
#include "util/size.h"

static void prv_push_welcome_notification(void *PBL_UNUSED data) {
  AttributeList notif_attr_list = {};
  attribute_list_add_uint32(&notif_attr_list, AttributeIdIconTiny,
                            TIMELINE_RESOURCE_NOTIFICATION_FLAG);
  attribute_list_add_cstring(&notif_attr_list, AttributeIdTitle,
                             /// Welcome title text welcoming a 3.x user to 4.x
                             i18n_get("Pebble Updated!", &notif_attr_list));
  /// Welcome body text welcoming a 3.x user to 4.x.
  const char *welcome_text = i18n_get(
      "For activity and sleep tracking, press up from your watch face.\n\n"
      "Press down for current and future events.\n\n"
      "Read more at blog.pebble.com",
      &notif_attr_list);
  attribute_list_add_cstring(&notif_attr_list, AttributeIdBody, welcome_text);
  attribute_list_add_uint8(&notif_attr_list, AttributeIdBgColor, GColorOrangeARGB8);

  AttributeList dismiss_action_attr_list = {};
  attribute_list_add_cstring(&dismiss_action_attr_list, AttributeIdTitle,
                             i18n_get("Dismiss", &notif_attr_list));

  int action_id = 0;
  TimelineItemAction actions[] = {
    {
      .id = action_id++,
      .type = TimelineItemActionTypeDismiss,
      .attr_list = dismiss_action_attr_list,
    },
  };
  TimelineItemActionGroup action_group = {
    .num_actions = ARRAY_LENGTH(actions),
    .actions = actions,
  };

  const time_t now = rtc_get_time();
  TimelineItem *item = timeline_item_create_with_attributes(
      now, 0, TimelineItemTypeNotification, LayoutIdNotification, &notif_attr_list, &action_group);
  i18n_free_all(&notif_attr_list);
  attribute_list_destroy_list(&notif_attr_list);
  attribute_list_destroy_list(&dismiss_action_attr_list);

  if (!item) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to welcome the user.");
    return;
  }

  item->header.from_watch = true;
  notifications_add_notification(item);
  timeline_item_destroy(item);
  welcome_set_welcome_version(WelcomeVersionCurrent);
}

void welcome_push_notification(bool factory_reset_or_first_use) {
  const WelcomeVersion version = welcome_get_welcome_version();
  // This check only works if it is called before getting started complete is set
  if (!factory_reset_or_first_use && (version < WelcomeVersion_4xNormalFirmware)) {
    // This has completed getting started on a previous normal firmware, welcome them if the
    // version is before 4.x
    // We wait some time since notification storage takes time to initialize
    launcher_task_add_callback(prv_push_welcome_notification, NULL);
  }
}
