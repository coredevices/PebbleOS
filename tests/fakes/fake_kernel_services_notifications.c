/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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


