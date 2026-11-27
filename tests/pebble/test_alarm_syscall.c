/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/alarms/alarm.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/notifications/notification_storage.h>

#include <clar.h>
#include <process_management/pebble_process_md.h>
#include <syscall/syscall.h>

static const PebbleProcessMd s_md = {
  .uuid = {
    0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4
  },
};

static bool s_read_granted;
static bool s_write_granted;
static bool s_can_schedule;
static int s_create_calls;
static AlarmCreateResult s_create_result;
static AlarmId s_created_id;
static AlarmGetResult s_info_result_code;
static AlarmInfo s_info_result;
static int s_info_calls;
static bool s_custom_days[PBL_DAY_PER_WEEK];
static AlarmSetEnabledResult s_set_enabled_result;
static bool s_set_enabled_value;
static int s_set_enabled_calls;
static bool s_next_alarm_enabled;
static time_t s_next_alarm_time;
static AlarmId s_ids[4];
static size_t s_ids_count;
static int s_for_each_calls;
static AlarmUpdateResult s_update_result;
static AlarmInfo s_updated_info;
static bool s_updated_days[PBL_DAY_PER_WEEK];
static int s_update_calls;
static AlarmDeleteResult s_delete_result;
static AlarmId s_deleted_id;
static int s_delete_calls;

const PebbleProcessMd *sys_process_manager_get_current_process_md(void) {
  return &s_md;
}

bool app_db_alarm_read_granted(const Uuid *uuid) {
  return s_read_granted && uuid_equal(uuid, &s_md.uuid);
}

bool app_db_alarm_write_granted(const Uuid *uuid) {
  return s_write_granted && uuid_equal(uuid, &s_md.uuid);
}

bool alarm_can_schedule(void) {
  return s_can_schedule;
}

bool alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  if (s_next_alarm_enabled && next_alarm_time_out) {
    *next_alarm_time_out = s_next_alarm_time;
  }
  return s_next_alarm_enabled;
}

AlarmCreateResult alarm_create_checked(const AlarmInfo *info, AlarmId *id_out) {
  s_create_calls++;
  if (s_create_result == AlarmCreateResultOk) {
    *id_out = s_created_id;
  }
  return s_create_result;
}

AlarmGetResult alarm_get_info_checked(AlarmId id, AlarmInfo *info_out,
                                      bool scheduled_days[PBL_DAY_PER_WEEK]) {
  (void)id;
  s_info_calls++;
  if (s_info_result_code != AlarmGetResultOk) {
    return s_info_result_code;
  }
  *info_out = s_info_result;
  memcpy(scheduled_days, s_custom_days, sizeof(s_custom_days));
  return AlarmGetResultOk;
}

AlarmSetEnabledResult alarm_set_enabled_checked(AlarmId id, bool enable) {
  (void)id;
  s_set_enabled_calls++;
  s_set_enabled_value = enable;
  return s_set_enabled_result;
}

AlarmUpdateResult alarm_update_checked(AlarmId id, const AlarmInfo *info) {
  (void)id;
  s_update_calls++;
  s_updated_info = *info;
  if (info->scheduled_days) {
    memcpy(s_updated_days, info->scheduled_days, sizeof(s_updated_days));
    s_updated_info.scheduled_days = &s_updated_days;
  }
  return s_update_result;
}

AlarmDeleteResult alarm_delete_checked(AlarmId id) {
  s_delete_calls++;
  s_deleted_id = id;
  return s_delete_result;
}

void alarm_for_each(AlarmForEach cb, void *context) {
  s_for_each_calls++;
  for (size_t i = 0; i < s_ids_count; ++i) {
    cb(s_ids[i], NULL, context);
  }
}

uint16_t notification_storage_get_count(void) {
  return 0;
}

static const AlarmServiceInfo s_info = {
  .hour = 7,
  .minute = 30,
  .kind = AlarmServiceKindWeekdays,
};

void test_alarm_syscall__initialize(void) {
  s_read_granted = true;
  s_write_granted = true;
  s_can_schedule = true;
  s_create_calls = 0;
  s_create_result = AlarmCreateResultOk;
  s_created_id = 42;
  s_info_result_code = AlarmGetResultOk;
  s_info_result = (AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_WEEKDAYS,
    .enabled = true,
    .is_smart = true,
    .sound_enabled = true,
    .vibrate_enabled = true,
    .tone = AlarmTone_Bell,
  };
  s_info_calls = 0;
  memset(s_custom_days, 0, sizeof(s_custom_days));
  s_set_enabled_result = AlarmSetEnabledResultOk;
  s_set_enabled_value = false;
  s_set_enabled_calls = 0;
  s_next_alarm_enabled = true;
  s_next_alarm_time = 1234567890;
  memset(s_ids, 0, sizeof(s_ids));
  s_ids_count = 0;
  s_for_each_calls = 0;
  s_update_result = AlarmUpdateResultOk;
  s_updated_info = (AlarmInfo){};
  memset(s_updated_days, 0, sizeof(s_updated_days));
  s_update_calls = 0;
  s_delete_result = AlarmDeleteResultOk;
  s_deleted_id = ALARM_INVALID_ID;
  s_delete_calls = 0;
}

void test_alarm_syscall__create_requires_permission(void) {
  s_write_granted = false;
  s_read_granted = true;

  AlarmServiceId id = 7;
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateErrPermissionDenied);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 0);
}

void test_alarm_syscall__create_rejects_invalid_custom_days(void) {
  AlarmServiceInfo info = s_info;
  info.kind = AlarmServiceKindCustom;

  AlarmServiceId id = 7;
  info.custom_days = 0;
  cl_assert_equal_i(sys_alarm_create(&info, &id), AlarmServiceCreateErrInvalidArgument);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 0);

  info.custom_days = 0x80;
  cl_assert_equal_i(sys_alarm_create(&info, &id), AlarmServiceCreateErrInvalidArgument);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 0);
}

void test_alarm_syscall__create_maps_invalid_argument(void) {
  s_create_result = AlarmCreateResultInvalidArgument;

  AlarmServiceId id = 7;
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateErrInvalidArgument);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_syscall__create_ignores_custom_days_for_other_kinds(void) {
  AlarmServiceInfo info = s_info;
  info.custom_days = 0x80;

  AlarmServiceId id = 0;
  cl_assert_equal_i(sys_alarm_create(&info, &id), AlarmServiceCreateOk);
  cl_assert_equal_i(id, s_created_id);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_syscall__create_maps_no_space(void) {
  s_create_result = AlarmCreateResultNoSpace;

  AlarmServiceId id = 7;
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateErrNoSpace);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_syscall__create_maps_storage_error_to_internal(void) {
  s_create_result = AlarmCreateResultStorageError;

  AlarmServiceId id = 7;
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateErrInternal);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_syscall__create_returns_id_on_success(void) {
  AlarmServiceId id = 7;
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateOk);
  cl_assert_equal_i(id, s_created_id);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_syscall__can_create_does_not_require_permission(void) {
  s_read_granted = false;
  s_write_granted = false;

  s_can_schedule = false;
  cl_assert(!sys_alarm_can_create());

  s_can_schedule = true;
  cl_assert(sys_alarm_can_create());
}

void test_alarm_syscall__get_requires_permission(void) {
  s_read_granted = false;
  s_write_granted = true;

  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(s_info_calls, 0);
}

void test_alarm_syscall__peek_requires_read_permission(void) {
  time_t timestamp = 0;

  s_read_granted = false;
  s_write_granted = true;
  cl_assert(!sys_alarm_get_next_enabled(&timestamp));

  s_read_granted = true;
  cl_assert(sys_alarm_get_next_enabled(&timestamp));
  cl_assert_equal_i(timestamp, s_next_alarm_time);
}

void test_alarm_syscall__get_ids_requires_read_permission(void) {
  s_read_granted = false;
  s_write_granted = true;
  s_ids_count = 2;

  AlarmServiceId ids[2] = {7, 8};
  cl_assert_equal_i(sys_alarm_get_ids(ids, 2), 0);
  cl_assert_equal_i(ids[0], 7);
  cl_assert_equal_i(ids[1], 8);
  cl_assert_equal_i(s_for_each_calls, 0);
}

void test_alarm_syscall__get_ids_passes_through(void) {
  s_ids_count = 2;
  s_ids[0] = 1;
  s_ids[1] = 4;

  AlarmServiceId ids[4] = {9, 9, 9, 9};
  cl_assert_equal_i(sys_alarm_get_ids(ids, 4), 2);
  cl_assert_equal_i(s_for_each_calls, 1);
  cl_assert_equal_i(ids[0], 1);
  cl_assert_equal_i(ids[1], 4);
  cl_assert_equal_i(ids[2], 9);
  cl_assert_equal_i(ids[3], 9);
}

void test_alarm_syscall__get_ids_truncates_to_max(void) {
  s_ids_count = 3;
  s_ids[0] = 1;
  s_ids[1] = 4;
  s_ids[2] = 7;

  AlarmServiceId ids[2] = {9, 9};
  cl_assert_equal_i(sys_alarm_get_ids(ids, 2), 3);
  cl_assert_equal_i(ids[0], 1);
  cl_assert_equal_i(ids[1], 4);
  cl_assert_equal_i(s_for_each_calls, 1);
}

void test_alarm_syscall__get_ids_can_only_count(void) {
  s_ids_count = 3;

  cl_assert_equal_i(sys_alarm_get_ids(NULL, 0), 3);
  cl_assert_equal_i(sys_alarm_get_ids(NULL, 5), 3);
  cl_assert_equal_i(s_for_each_calls, 2);
}

void test_alarm_syscall__read_and_write_permissions_are_separate(void) {
  AlarmServiceAlarm alarm = {};
  AlarmServiceId id = 0;
  AlarmServiceId ids[1] = {};
  time_t timestamp = 0;
  s_ids_count = 1;

  s_read_granted = true;
  s_write_granted = false;
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultOk);
  cl_assert_equal_i(sys_alarm_get_ids(ids, 1), 1);
  cl_assert(sys_alarm_get_next_enabled(&timestamp));
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateErrPermissionDenied);
  cl_assert_equal_i(sys_alarm_set_enabled(2, true), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(sys_alarm_delete(2), AlarmServiceResultErrPermissionDenied);

  s_read_granted = false;
  s_write_granted = true;
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(sys_alarm_get_ids(ids, 1), 0);
  cl_assert(!sys_alarm_get_next_enabled(&timestamp));
  cl_assert_equal_i(sys_alarm_create(&s_info, &id), AlarmServiceCreateOk);
  cl_assert_equal_i(sys_alarm_set_enabled(2, true), AlarmServiceResultOk);
  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultOk);
  cl_assert_equal_i(sys_alarm_delete(2), AlarmServiceResultOk);
}

void test_alarm_syscall__rejects_out_of_range_id(void) {
  AlarmServiceAlarm alarm = {};

  cl_assert_equal_i(sys_alarm_get(-1, &alarm), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(sys_alarm_get(ALARM_SERVICE_MAX_ALARMS, &alarm),
                    AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(sys_alarm_set_enabled(-1, true), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(sys_alarm_set_enabled(ALARM_SERVICE_MAX_ALARMS, true),
                    AlarmServiceResultErrInvalidArgument);

  cl_assert_equal_i(s_info_calls, 0);
  cl_assert_equal_i(s_set_enabled_calls, 0);
}

void test_alarm_syscall__get_returns_alarm(void) {
  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultOk);
  cl_assert_equal_i(alarm.id, 2);
  cl_assert(alarm.enabled);
  cl_assert_equal_i(alarm.info.hour, 7);
  cl_assert_equal_i(alarm.info.minute, 30);
  cl_assert_equal_i(alarm.info.kind, AlarmServiceKindWeekdays);
  cl_assert(alarm.info.smart);
  cl_assert(alarm.info.sound);
  cl_assert(alarm.info.vibrate);
  cl_assert_equal_i(alarm.info.tone, AlarmServiceToneBell);
  cl_assert_equal_i(alarm.info.custom_days, 0);
}

void test_alarm_syscall__get_zeroes_padding(void) {
  AlarmServiceAlarm alarm;
  memset(&alarm, 0xff, sizeof(alarm));

  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultOk);

  AlarmServiceAlarm expected;
  memset(&expected, 0, sizeof(expected));
  expected.id = 2;
  expected.enabled = s_info_result.enabled;
  expected.info.hour = s_info_result.hour;
  expected.info.minute = s_info_result.minute;
  expected.info.kind = (AlarmServiceKind)s_info_result.kind;
  expected.info.smart = s_info_result.is_smart;
  expected.info.sound = s_info_result.sound_enabled;
  expected.info.vibrate = s_info_result.vibrate_enabled;
  expected.info.tone = (AlarmServiceTone)s_info_result.tone;

  cl_assert_equal_i(memcmp(&alarm, &expected, sizeof(alarm)), 0);
}

void test_alarm_syscall__get_returns_custom_days(void) {
  s_info_result.kind = ALARM_KIND_CUSTOM;
  s_custom_days[0] = true;
  s_custom_days[6] = true;

  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultOk);
  cl_assert_equal_i(alarm.info.custom_days, 0x41);
}

void test_alarm_syscall__get_missing_alarm(void) {
  s_info_result_code = AlarmGetResultDoesNotExist;

  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultErrDoesNotExist);
}

void test_alarm_syscall__get_maps_storage_error(void) {
  s_info_result_code = AlarmGetResultStorageError;

  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(sys_alarm_get(2, &alarm), AlarmServiceResultErrInternal);
}

void test_alarm_syscall__set_enabled_requires_permission(void) {
  s_write_granted = false;
  s_read_granted = true;

  cl_assert_equal_i(sys_alarm_set_enabled(2, true), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(s_set_enabled_calls, 0);
}

void test_alarm_syscall__set_enabled_maps_results(void) {
  cl_assert_equal_i(sys_alarm_set_enabled(2, true), AlarmServiceResultOk);
  cl_assert(s_set_enabled_value);
  cl_assert_equal_i(s_set_enabled_calls, 1);

  s_set_enabled_result = AlarmSetEnabledResultDoesNotExist;
  cl_assert_equal_i(sys_alarm_set_enabled(2, false), AlarmServiceResultErrDoesNotExist);

  s_set_enabled_result = AlarmSetEnabledResultStorageError;
  cl_assert_equal_i(sys_alarm_set_enabled(2, false), AlarmServiceResultErrInternal);
}

void test_alarm_syscall__update_requires_write_permission(void) {
  s_read_granted = true;
  s_write_granted = false;

  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(s_update_calls, 0);
}

void test_alarm_syscall__update_rejects_out_of_range_id(void) {
  cl_assert_equal_i(sys_alarm_update(-1, &s_info), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(sys_alarm_update(ALARM_SERVICE_MAX_ALARMS, &s_info),
                    AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(s_update_calls, 0);
}

void test_alarm_syscall__update_rejects_invalid_custom_days(void) {
  AlarmServiceInfo info = s_info;
  info.kind = AlarmServiceKindCustom;

  info.custom_days = 0;
  cl_assert_equal_i(sys_alarm_update(2, &info), AlarmServiceResultErrInvalidArgument);

  info.custom_days = 0x80;
  cl_assert_equal_i(sys_alarm_update(2, &info), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(s_update_calls, 0);
}

void test_alarm_syscall__update_passes_configuration(void) {
  const AlarmServiceInfo info = {
    .hour = 7,
    .minute = 15,
    .kind = AlarmServiceKindCustom,
    .custom_days = 0x41,
    .smart = true,
    .sound = true,
    .vibrate = true,
    .tone = AlarmServiceToneBell,
  };

  cl_assert_equal_i(sys_alarm_update(2, &info), AlarmServiceResultOk);
  cl_assert_equal_i(s_update_calls, 1);
  cl_assert_equal_i(s_updated_info.hour, 7);
  cl_assert_equal_i(s_updated_info.minute, 15);
  cl_assert_equal_i(s_updated_info.kind, ALARM_KIND_CUSTOM);
  cl_assert(s_updated_info.scheduled_days);
  cl_assert((*s_updated_info.scheduled_days)[0]);
  cl_assert(!(*s_updated_info.scheduled_days)[1]);
  cl_assert((*s_updated_info.scheduled_days)[6]);
  cl_assert(s_updated_info.is_smart);
  cl_assert(s_updated_info.sound_enabled);
  cl_assert(s_updated_info.vibrate_enabled);
  cl_assert_equal_i(s_updated_info.tone, AlarmTone_Bell);
}

void test_alarm_syscall__update_maps_results(void) {
  s_update_result = AlarmUpdateResultDoesNotExist;
  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultErrDoesNotExist);

  s_update_result = AlarmUpdateResultInvalidArgument;
  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultErrInvalidArgument);

  s_update_result = AlarmUpdateResultStorageError;
  cl_assert_equal_i(sys_alarm_update(2, &s_info), AlarmServiceResultErrInternal);
  cl_assert_equal_i(s_update_calls, 3);
}

void test_alarm_syscall__delete_requires_write_permission(void) {
  s_read_granted = true;
  s_write_granted = false;

  cl_assert_equal_i(sys_alarm_delete(2), AlarmServiceResultErrPermissionDenied);
  cl_assert_equal_i(s_delete_calls, 0);
}

void test_alarm_syscall__delete_rejects_out_of_range_id(void) {
  cl_assert_equal_i(sys_alarm_delete(-1), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(sys_alarm_delete(ALARM_SERVICE_MAX_ALARMS),
                    AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(s_delete_calls, 0);
}

void test_alarm_syscall__delete_passes_through(void) {
  cl_assert_equal_i(sys_alarm_delete(3), AlarmServiceResultOk);
  cl_assert_equal_i(s_delete_calls, 1);
  cl_assert_equal_i(s_deleted_id, 3);
}

void test_alarm_syscall__delete_maps_results(void) {
  s_delete_result = AlarmDeleteResultDoesNotExist;
  cl_assert_equal_i(sys_alarm_delete(2), AlarmServiceResultErrDoesNotExist);

  s_delete_result = AlarmDeleteResultStorageError;
  cl_assert_equal_i(sys_alarm_delete(2), AlarmServiceResultErrInternal);
}
