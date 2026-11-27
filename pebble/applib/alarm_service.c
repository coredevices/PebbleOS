/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "alarm_service.h"

#include <pbl/services/alarms/alarm.h>

#include <syscall/syscall.h>

static_assert((int)AlarmServiceKindEveryday == (int)ALARM_KIND_EVERYDAY, "AlarmKind mismatch");
static_assert((int)AlarmServiceKindWeekends == (int)ALARM_KIND_WEEKENDS, "AlarmKind mismatch");
static_assert((int)AlarmServiceKindWeekdays == (int)ALARM_KIND_WEEKDAYS, "AlarmKind mismatch");
static_assert((int)AlarmServiceKindJustOnce == (int)ALARM_KIND_JUST_ONCE, "AlarmKind mismatch");
static_assert((int)AlarmServiceKindCustom == (int)ALARM_KIND_CUSTOM, "AlarmKind mismatch");
static_assert((int)AlarmServiceToneReveille == (int)AlarmTone_Reveille, "AlarmTone mismatch");
static_assert((int)AlarmServiceToneBeacon == (int)AlarmTone_Beacon, "AlarmTone mismatch");
static_assert((int)AlarmServiceToneBell == (int)AlarmTone_Bell, "AlarmTone mismatch");
static_assert((int)AlarmServiceToneChime == (int)AlarmTone_Chime, "AlarmTone mismatch");
// The public structs contain enums, so their layout follows the compiler's
// enum width. ARM EABI and the native build use short enums; the unit test
// build does not.
static_assert(sizeof(AlarmServiceKind) == sizeof(AlarmServiceTone),
              "AlarmService enum width mismatch");
static_assert(sizeof(AlarmServiceKind) == 1 || sizeof(AlarmServiceKind) == 4,
              "unexpected AlarmService enum width");
static_assert(sizeof(AlarmServiceInfo) == (sizeof(AlarmServiceKind) == 1 ? 8 : 16),
              "AlarmServiceInfo is frozen");
static_assert(sizeof(AlarmServiceAlarm) == (sizeof(AlarmServiceKind) == 1 ? 16 : 24),
              "AlarmServiceAlarm is frozen");

bool alarm_service_peek_next(pbl_time32_t *timestamp_out) {
  if (!timestamp_out) {
    return false;
  }

  time_t t;
  if (!sys_alarm_get_next_enabled(&t)) {
    return false;
  }
  *timestamp_out = pbl_time_to_time32(t);
  return true;
}

bool alarm_service_can_create(void) {
  return sys_alarm_can_create();
}

AlarmServiceCreateResult alarm_service_create(const AlarmServiceInfo *info,
                                              AlarmServiceId *id_out) {
  if (id_out) {
    *id_out = ALARM_SERVICE_INVALID_ID;
  }
  if (!info || !id_out) {
    return AlarmServiceCreateErrInvalidArgument;
  }

  return sys_alarm_create(info, id_out);
}

AlarmServiceResult alarm_service_get(AlarmServiceId id, AlarmServiceAlarm *alarm_out) {
  if (!alarm_out) {
    return AlarmServiceResultErrInvalidArgument;
  }

  return sys_alarm_get(id, alarm_out);
}

AlarmServiceResult alarm_service_update(AlarmServiceId id, const AlarmServiceInfo *info) {
  if (!info) {
    return AlarmServiceResultErrInvalidArgument;
  }

  return sys_alarm_update(id, info);
}

size_t alarm_service_get_ids(AlarmServiceId *ids, size_t max) {
  return sys_alarm_get_ids(ids, max);
}

AlarmServiceResult alarm_service_set_enabled(AlarmServiceId id, bool enable) {
  return sys_alarm_set_enabled(id, enable);
}

AlarmServiceResult alarm_service_delete(AlarmServiceId id) {
  return sys_alarm_delete(id);
}
