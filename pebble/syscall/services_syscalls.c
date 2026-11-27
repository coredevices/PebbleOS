/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/kernel/syscall.h>
#include <pbl/services/alarms/alarm.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/notifications/notification_storage.h>

#ifdef CONFIG_PRESSURE
#include <pbl/drivers/pressure.h>
#endif

#include <process_management/pebble_process_md.h>
#include <syscall/syscall.h>

static_assert(sizeof(AlarmServiceId) == sizeof(AlarmId),
              "AlarmServiceId and AlarmId must have the same size");

static bool prv_alarm_read_permission_granted(void) {
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  return md && app_db_alarm_read_granted(&md->uuid);
}

static bool prv_alarm_write_permission_granted(void) {
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  return md && app_db_alarm_write_granted(&md->uuid);
}

PBL_SYSCALL(bool, sys_alarm_get_next_enabled, (OUT, time_t *, timestamp_out)) {
  if (!prv_alarm_read_permission_granted()) {
    return false;
  }
  return alarm_get_next_enabled_alarm(timestamp_out);
}

PBL_SYSCALL(bool, sys_alarm_can_create) {
  return alarm_can_schedule();
}

static bool prv_alarm_custom_days_valid(const AlarmServiceInfo *info) {
  return info->kind != AlarmServiceKindCustom ||
         (info->custom_days != 0 && !(info->custom_days & ~0x7f));
}

static AlarmInfo prv_alarm_info_from_service(const AlarmServiceInfo *info,
                                             bool (*scheduled_days)[PBL_DAY_PER_WEEK]) {
  for (int i = 0; i < PBL_DAY_PER_WEEK; ++i) {
    (*scheduled_days)[i] = (info->custom_days & (1u << i)) != 0;
  }

  return (AlarmInfo){
    .hour = info->hour,
    .minute = info->minute,
    .kind = (AlarmKind)info->kind,
    .scheduled_days = scheduled_days,
    .enabled = true,
    .is_smart = info->smart,
    .sound_enabled = info->sound,
    .vibrate_enabled = info->vibrate,
    .tone = (AlarmTone)info->tone,
  };
}

PBL_SYSCALL(AlarmServiceCreateResult, sys_alarm_create, (IN, const AlarmServiceInfo *, info),
            (OUT, AlarmServiceId *, id_out)) {
  *id_out = ALARM_SERVICE_INVALID_ID;
  if (!prv_alarm_write_permission_granted()) {
    return AlarmServiceCreateErrPermissionDenied;
  }

  if (!prv_alarm_custom_days_valid(info)) {
    return AlarmServiceCreateErrInvalidArgument;
  }

  bool scheduled_days[PBL_DAY_PER_WEEK];
  const AlarmInfo alarm_info = prv_alarm_info_from_service(info, &scheduled_days);

  AlarmId id = ALARM_INVALID_ID;
  switch (alarm_create_checked(&alarm_info, &id)) {
    case AlarmCreateResultOk:
      *id_out = id;
      return AlarmServiceCreateOk;
    case AlarmCreateResultNoSpace:
      return AlarmServiceCreateErrNoSpace;
    case AlarmCreateResultInvalidArgument:
      return AlarmServiceCreateErrInvalidArgument;
    case AlarmCreateResultStorageError:
      return AlarmServiceCreateErrInternal;
  }
  return AlarmServiceCreateErrInternal;
}

PBL_SYSCALL(AlarmServiceResult, sys_alarm_get, (VAL, AlarmServiceId, id),
            (OUT, AlarmServiceAlarm *, alarm_out)) {
  if (!prv_alarm_read_permission_granted()) {
    return AlarmServiceResultErrPermissionDenied;
  }
  if (id < 0 || id >= ALARM_SERVICE_MAX_ALARMS) {
    return AlarmServiceResultErrInvalidArgument;
  }

  AlarmInfo info;
  bool scheduled_days[PBL_DAY_PER_WEEK];
  switch (alarm_get_info_checked(id, &info, scheduled_days)) {
    case AlarmGetResultOk:
      break;
    case AlarmGetResultDoesNotExist:
      return AlarmServiceResultErrDoesNotExist;
    case AlarmGetResultInvalidArgument:
      return AlarmServiceResultErrInvalidArgument;
    case AlarmGetResultStorageError:
      return AlarmServiceResultErrInternal;
  }

  memset(alarm_out, 0, sizeof(*alarm_out));
  alarm_out->id = id;
  alarm_out->enabled = info.enabled;
  alarm_out->info.hour = info.hour;
  alarm_out->info.minute = info.minute;
  alarm_out->info.kind = (AlarmServiceKind)info.kind;
  alarm_out->info.smart = info.is_smart;
  alarm_out->info.sound = info.sound_enabled;
  alarm_out->info.vibrate = info.vibrate_enabled;
  alarm_out->info.tone = (AlarmServiceTone)info.tone;

  if (info.kind == ALARM_KIND_CUSTOM) {
    for (int i = 0; i < PBL_DAY_PER_WEEK; ++i) {
      if (scheduled_days[i]) {
        alarm_out->info.custom_days |= (1u << i);
      }
    }
  }

  return AlarmServiceResultOk;
}

typedef struct {
  AlarmServiceId *ids;
  size_t max;
  size_t count;
} AlarmIdList;

static void prv_collect_alarm_id(AlarmId id, const AlarmInfo *info, void *context) {
  (void)info;
  AlarmIdList *list = context;
  if (list->ids && list->count < list->max) {
    list->ids[list->count] = (AlarmServiceId)id;
  }
  list->count++;
}

PBL_SYSCALL(size_t, sys_alarm_get_ids, (VAL, AlarmServiceId *, ids), (VAL, size_t, max)) {
  if (!prv_alarm_read_permission_granted()) {
    return 0;
  }
  if (from_user && ids && max > 0) {
    size_t num_bytes;
    if (ckd_mul(&num_bytes, max, sizeof(*ids))) {
      pbl_syscall_fail();
    }
    pbl_syscall_verify_user(ids, num_bytes);
  }
  AlarmIdList list = {
    .ids = ids,
    .max = max,
  };
  alarm_for_each(prv_collect_alarm_id, &list);
  return list.count;
}

PBL_SYSCALL(AlarmServiceResult, sys_alarm_set_enabled, (VAL, AlarmServiceId, id),
            (VAL, bool, enabled)) {
  if (!prv_alarm_write_permission_granted()) {
    return AlarmServiceResultErrPermissionDenied;
  }
  if (id < 0 || id >= ALARM_SERVICE_MAX_ALARMS) {
    return AlarmServiceResultErrInvalidArgument;
  }

  switch (alarm_set_enabled_checked(id, enabled)) {
    case AlarmSetEnabledResultOk:
      return AlarmServiceResultOk;
    case AlarmSetEnabledResultDoesNotExist:
      return AlarmServiceResultErrDoesNotExist;
    case AlarmSetEnabledResultStorageError:
      return AlarmServiceResultErrInternal;
  }
  return AlarmServiceResultErrInternal;
}

PBL_SYSCALL(AlarmServiceResult, sys_alarm_update, (VAL, AlarmServiceId, id),
            (IN, const AlarmServiceInfo *, info)) {
  if (!prv_alarm_write_permission_granted()) {
    return AlarmServiceResultErrPermissionDenied;
  }
  if (id < 0 || id >= ALARM_SERVICE_MAX_ALARMS) {
    return AlarmServiceResultErrInvalidArgument;
  }
  if (!prv_alarm_custom_days_valid(info)) {
    return AlarmServiceResultErrInvalidArgument;
  }

  bool scheduled_days[PBL_DAY_PER_WEEK];
  const AlarmInfo alarm_info = prv_alarm_info_from_service(info, &scheduled_days);
  switch (alarm_update_checked(id, &alarm_info)) {
    case AlarmUpdateResultOk:
      return AlarmServiceResultOk;
    case AlarmUpdateResultDoesNotExist:
      return AlarmServiceResultErrDoesNotExist;
    case AlarmUpdateResultInvalidArgument:
      return AlarmServiceResultErrInvalidArgument;
    case AlarmUpdateResultStorageError:
      return AlarmServiceResultErrInternal;
  }
  return AlarmServiceResultErrInternal;
}

PBL_SYSCALL(AlarmServiceResult, sys_alarm_delete, (VAL, AlarmServiceId, id)) {
  if (!prv_alarm_write_permission_granted()) {
    return AlarmServiceResultErrPermissionDenied;
  }
  if (id < 0 || id >= ALARM_SERVICE_MAX_ALARMS) {
    return AlarmServiceResultErrInvalidArgument;
  }

  switch (alarm_delete_checked(id)) {
    case AlarmDeleteResultOk:
      return AlarmServiceResultOk;
    case AlarmDeleteResultDoesNotExist:
      return AlarmServiceResultErrDoesNotExist;
    case AlarmDeleteResultStorageError:
      return AlarmServiceResultErrInternal;
  }
  return AlarmServiceResultErrInternal;
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
