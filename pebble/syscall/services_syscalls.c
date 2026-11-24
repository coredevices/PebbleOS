/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/kernel/syscall.h>
#include <pbl/services/alarms/alarm.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/notifications/notification_storage.h>
#include <pbl/services/timers/timer.h>

#ifdef CONFIG_PRESSURE
#include <pbl/drivers/pressure.h>
#endif

#include <process_management/pebble_process_md.h>
#include <syscall/syscall.h>

PBL_SYSCALL(bool, sys_alarm_get_next_enabled, (OUT, time_t *, timestamp_out)) {
  return alarm_get_next_enabled_alarm(timestamp_out);
}

PBL_SYSCALL(bool, sys_timer_can_create) {
#ifdef CONFIG_SERVICE_TIMERS
  return timer_can_create();
#else
  return false;
#endif
}

PBL_SYSCALL(TimerServiceCreateResult, sys_timer_create, (VAL, uint32_t, duration_s),
            (OUT, TimerServiceId *, id_out)) {
#ifdef CONFIG_SERVICE_TIMERS
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  if (!md || !app_db_timer_granted(&md->uuid)) {
    return TimerServiceCreateErrPermissionDenied;
  }

  if (duration_s == 0 || duration_s > TIMER_SERVICE_MAX_DURATION_S) {
    return TimerServiceCreateErrInvalidArgument;
  }

  const TimerId id = timer_start(duration_s, &md->uuid);
  if (id == TIMER_INVALID_ID) {
    return TimerServiceCreateErrNoSpace;
  }

  *id_out = id;
  return TimerServiceCreateOk;
#else
  return TimerServiceCreateErrUnavailable;
#endif
}

PBL_SYSCALL(bool, sys_timer_cancel, (VAL, TimerServiceId, id)) {
#ifdef CONFIG_SERVICE_TIMERS
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  if (!md) {
    return false;
  }

  return timer_cancel(id, &md->uuid);
#else
  return false;
#endif
}

PBL_SYSCALL(uint32_t, sys_timer_cancel_all) {
#ifdef CONFIG_SERVICE_TIMERS
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  if (!md) {
    return 0;
  }

  return timer_cancel_all(&md->uuid);
#else
  return 0;
#endif
}

PBL_SYSCALL(uint8_t, sys_timer_get_timers, (OUT_ARRAY, TimerServiceInfo *, timers, max_timers),
            (VAL, uint8_t, max_timers)) {
  if (!timers || max_timers == 0) {
    return 0;
  }
#ifdef CONFIG_SERVICE_TIMERS
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  if (!md) {
    return 0;
  }

  TimerInfo infos[TIMER_MAX_COUNT];
  const uint8_t count = timer_get_timers(&md->uuid, infos, TIMER_MAX_COUNT);
  const uint8_t written = count < max_timers ? count : max_timers;
  for (uint8_t i = 0; i < written; ++i) {
    timers[i] = (TimerServiceInfo){
      .id = infos[i].id,
      .duration_s = infos[i].duration_s,
      .remaining_s = infos[i].remaining_s,
    };
    memcpy(timers[i].description, infos[i].description, sizeof(infos[i].description));
  }
  return written;
#else
  return 0;
#endif
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
