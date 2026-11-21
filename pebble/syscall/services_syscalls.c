/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/syscall.h>
#include <pbl/services/alarms/alarm.h>
#include <pbl/services/notifications/notification_storage.h>

#ifdef CONFIG_PRESSURE
#include <pbl/drivers/pressure.h>
#endif

PBL_SYSCALL(bool, sys_alarm_get_next_enabled, (OUT, time_t *, timestamp_out)) {
  return alarm_get_next_enabled_alarm(timestamp_out);
}

PBL_SYSCALL(bool, sys_hrm_manager_is_hrm_present) {
#ifdef CONFIG_SERVICE_HRM
  return true;
#else
  return false;
#endif
}

PBL_SYSCALL(uint16_t, sys_notification_service_get_count) {
#ifdef CONFIG_SERVICE_NOTIFICATIONS
  return notification_storage_get_count();
#else
  return 0;
#endif
}

PBL_SYSCALL(int, sys_pressure_read, (OUT, uint32_t *, pa)) {
#ifdef CONFIG_PRESSURE
  return pbl_pressure_read(PRESSURE, pa);
#else
  return -ENODEV;
#endif
}
