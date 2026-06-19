/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall_internal.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/alarms/alarm.h"
#include "pbl/services/tick_timer.h"

DEFINE_SYSCALL(bool, sys_alarm_get_next_enabled, time_t *timestamp_out) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(timestamp_out, sizeof(*timestamp_out));
  }
  return alarm_get_next_enabled_alarm(timestamp_out);
}

DEFINE_SYSCALL(bool, sys_hrm_manager_is_hrm_present) {
#ifdef CONFIG_SERVICE_HRM
  return true;
#else
  return false;
#endif
}

DEFINE_SYSCALL(void, sys_tick_timer_set_seconds_subscribed, bool needs_seconds) {
  tick_timer_set_seconds_subscribed(pebble_task_get_current(), needs_seconds);
}
