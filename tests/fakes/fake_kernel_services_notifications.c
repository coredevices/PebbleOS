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

#include "fake_kernel_services_notifications.h"

#include "services/normal/notifications/notification_storage.h"

static uint32_t s_ancs_count = 0;
static uint32_t s_acted_upon_count = 0;

void notifications_handle_notification_added(Uuid *id) {
  ++s_ancs_count;
}

void notifications_handle_notification_removed(Uuid *id) {
  --s_ancs_count;
}

void notifications_handle_notification_acted_upon(Uuid *id) {
  ++s_acted_upon_count;
  return;
}

void notifications_handle_notification_action_result(PebbleSysNotificationActionResult *action_result) {
}

void notifications_add_notification(TimelineItem *notification) {
  notification_storage_store(notification);
  ++s_ancs_count;
}

void fake_kernel_services_notifications_reset(void) {
  s_ancs_count = 0;
  s_acted_upon_count = 0;
}

uint32_t fake_kernel_services_notifications_ancs_notifications_count(void) {
  return s_ancs_count;
}

uint32_t fake_kernel_services_notifications_acted_upon_count(void) {
  return s_acted_upon_count;
}


