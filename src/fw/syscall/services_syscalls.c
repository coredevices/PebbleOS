/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall_internal.h"

#include "pbl/services/alarms/alarm.h"
#include "pbl/services/notifications/notification_storage.h"

DEFINE_SYSCALL(bool, sys_alarm_get_next_enabled, time_t *timestamp_out) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(timestamp_out, sizeof(*timestamp_out));
  }
  return alarm_get_next_enabled_alarm(timestamp_out);
}

DEFINE_SYSCALL(uint8_t, sys_notification_get_unread_count) {
  // No userspace buffer to validate: the count is returned by value.
#ifdef CONFIG_SERVICE_NOTIFICATIONS
  return notification_storage_get_unread_count();
#else
  return 0;
#endif
}

DEFINE_SYSCALL(bool, sys_hrm_manager_is_hrm_present) {
#ifdef CONFIG_SERVICE_HRM
  return true;
#else
  return false;
#endif
}