/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <applib/alarm_service.h>
#include <clar.h>
#include <syscall/syscall.h>

static AlarmServiceCreateResult s_create_result;
static AlarmServiceId s_create_id;
static int s_create_calls;
static AlarmServiceResult s_get_result;
static AlarmServiceAlarm s_get_alarm;
static int s_get_calls;
static AlarmServiceId s_get_ids_ids[4];
static size_t s_get_ids_count;
static size_t s_get_ids_max;
static int s_get_ids_calls;
static AlarmServiceResult s_set_enabled_result;
static bool s_set_enabled_value;
static int s_set_enabled_calls;
static AlarmServiceResult s_update_result;
static AlarmServiceInfo s_update_info;
static int s_update_calls;
static AlarmServiceResult s_delete_result;
static AlarmServiceId s_delete_id;
static int s_delete_calls;
static bool s_can_create;
static bool s_peek;
static time_t s_peek_time;

AlarmServiceCreateResult sys_alarm_create(const AlarmServiceInfo *info, AlarmServiceId *id_out) {
  (void)info;
  s_create_calls++;
  if (s_create_result == AlarmServiceCreateOk) {
    *id_out = s_create_id;
  }
  return s_create_result;
}

AlarmServiceResult sys_alarm_get(AlarmServiceId id, AlarmServiceAlarm *alarm_out) {
  (void)id;
  s_get_calls++;
  if (s_get_result == AlarmServiceResultOk) {
    *alarm_out = s_get_alarm;
  }
  return s_get_result;
}

size_t sys_alarm_get_ids(AlarmServiceId *ids, size_t max) {
  s_get_ids_calls++;
  s_get_ids_max = max;
  if (ids) {
    for (size_t i = 0; i < s_get_ids_count && i < max; ++i) {
      ids[i] = s_get_ids_ids[i];
    }
  }
  return s_get_ids_count;
}

AlarmServiceResult sys_alarm_set_enabled(AlarmServiceId id, bool enabled) {
  (void)id;
  s_set_enabled_calls++;
  s_set_enabled_value = enabled;
  return s_set_enabled_result;
}

AlarmServiceResult sys_alarm_update(AlarmServiceId id, const AlarmServiceInfo *info) {
  (void)id;
  s_update_calls++;
  s_update_info = *info;
  return s_update_result;
}

AlarmServiceResult sys_alarm_delete(AlarmServiceId id) {
  s_delete_calls++;
  s_delete_id = id;
  return s_delete_result;
}

bool sys_alarm_can_create(void) {
  return s_can_create;
}

bool sys_alarm_get_next_enabled(time_t *timestamp_out) {
  if (!s_peek) {
    return false;
  }
  *timestamp_out = s_peek_time;
  return true;
}

void test_alarm_service__initialize(void) {
  s_create_result = AlarmServiceCreateOk;
  s_create_id = ALARM_SERVICE_INVALID_ID;
  s_create_calls = 0;
  s_get_result = AlarmServiceResultOk;
  s_get_alarm = (AlarmServiceAlarm){};
  s_get_calls = 0;
  memset(s_get_ids_ids, 0, sizeof(s_get_ids_ids));
  s_get_ids_count = 0;
  s_get_ids_max = 0;
  s_get_ids_calls = 0;
  s_set_enabled_result = AlarmServiceResultOk;
  s_set_enabled_value = false;
  s_set_enabled_calls = 0;
  s_update_result = AlarmServiceResultOk;
  s_update_info = (AlarmServiceInfo){};
  s_update_calls = 0;
  s_delete_result = AlarmServiceResultOk;
  s_delete_id = ALARM_SERVICE_INVALID_ID;
  s_delete_calls = 0;
  s_can_create = false;
  s_peek = false;
  s_peek_time = 0;
}

void test_alarm_service__create_sets_id_on_success(void) {
  const AlarmServiceInfo info = {
    .hour = 7,
    .minute = 30,
    .kind = AlarmServiceKindWeekdays,
  };
  s_create_id = 7;

  AlarmServiceId id = 42;
  cl_assert_equal_i(alarm_service_create(&info, &id), AlarmServiceCreateOk);
  cl_assert_equal_i(id, 7);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_service__create_invalidates_id_on_failure(void) {
  const AlarmServiceInfo info = {
    .hour = 7,
    .minute = 30,
    .kind = AlarmServiceKindWeekdays,
  };
  s_create_result = AlarmServiceCreateErrNoSpace;

  AlarmServiceId id = 42;
  cl_assert_equal_i(alarm_service_create(&info, &id), AlarmServiceCreateErrNoSpace);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(s_create_calls, 1);
}

void test_alarm_service__create_rejects_null_arguments(void) {
  const AlarmServiceInfo info = {
    .hour = 7,
    .minute = 30,
    .kind = AlarmServiceKindWeekdays,
  };

  AlarmServiceId id = 42;
  cl_assert_equal_i(alarm_service_create(NULL, &id), AlarmServiceCreateErrInvalidArgument);
  cl_assert_equal_i(id, ALARM_SERVICE_INVALID_ID);
  cl_assert_equal_i(alarm_service_create(&info, NULL), AlarmServiceCreateErrInvalidArgument);
  cl_assert_equal_i(s_create_calls, 0);
}

void test_alarm_service__can_create_passes_through(void) {
  s_can_create = true;
  cl_assert(alarm_service_can_create());

  s_can_create = false;
  cl_assert(!alarm_service_can_create());
}

void test_alarm_service__peek_next(void) {
  pbl_time32_t timestamp = 0;
  cl_assert(!alarm_service_peek_next(&timestamp));

  s_peek = true;
  s_peek_time = 1234;
  cl_assert(alarm_service_peek_next(&timestamp));
  cl_assert_equal_i(timestamp, 1234);
}

void test_alarm_service__peek_next_rejects_null(void) {
  cl_assert(!alarm_service_peek_next(NULL));

  s_peek = true;
  cl_assert(!alarm_service_peek_next(NULL));
}

void test_alarm_service__get_passes_through(void) {
  s_get_alarm = (AlarmServiceAlarm){
    .id = 3,
    .enabled = true,
    .info = {.hour = 7, .minute = 30, .kind = AlarmServiceKindWeekdays},
  };

  AlarmServiceAlarm alarm = {};
  cl_assert_equal_i(alarm_service_get(3, &alarm), AlarmServiceResultOk);
  cl_assert_equal_i(alarm.id, 3);
  cl_assert(alarm.enabled);
  cl_assert_equal_i(alarm.info.hour, 7);
  cl_assert_equal_i(s_get_calls, 1);
}

void test_alarm_service__get_rejects_null_output(void) {
  cl_assert_equal_i(alarm_service_get(3, NULL), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(s_get_calls, 0);
}

void test_alarm_service__get_ids_passes_through(void) {
  s_get_ids_count = 3;
  s_get_ids_ids[0] = 2;
  s_get_ids_ids[1] = 5;
  s_get_ids_ids[2] = 8;

  AlarmServiceId ids[2] = {};
  cl_assert_equal_i(alarm_service_get_ids(ids, 2), 3);
  cl_assert_equal_i(s_get_ids_calls, 1);
  cl_assert_equal_i(s_get_ids_max, 2);
  cl_assert_equal_i(ids[0], 2);
  cl_assert_equal_i(ids[1], 5);
}

void test_alarm_service__get_ids_can_only_count(void) {
  s_get_ids_count = 4;

  cl_assert_equal_i(alarm_service_get_ids(NULL, 0), 4);
  cl_assert_equal_i(s_get_ids_calls, 1);
  cl_assert_equal_i(s_get_ids_max, 0);
}

void test_alarm_service__set_enabled_passes_through(void) {
  s_set_enabled_result = AlarmServiceResultErrDoesNotExist;

  cl_assert_equal_i(alarm_service_set_enabled(3, true), AlarmServiceResultErrDoesNotExist);
  cl_assert(s_set_enabled_value);
  cl_assert_equal_i(s_set_enabled_calls, 1);
}

void test_alarm_service__update_rejects_null_info(void) {
  cl_assert_equal_i(alarm_service_update(3, NULL), AlarmServiceResultErrInvalidArgument);
  cl_assert_equal_i(s_update_calls, 0);
}

void test_alarm_service__update_passes_through(void) {
  s_update_result = AlarmServiceResultErrDoesNotExist;
  const AlarmServiceInfo info = {
    .hour = 7,
    .minute = 15,
    .kind = AlarmServiceKindWeekdays,
  };

  cl_assert_equal_i(alarm_service_update(3, &info), AlarmServiceResultErrDoesNotExist);
  cl_assert_equal_i(s_update_calls, 1);
  cl_assert_equal_i(s_update_info.hour, 7);
  cl_assert_equal_i(s_update_info.minute, 15);
  cl_assert_equal_i(s_update_info.kind, AlarmServiceKindWeekdays);
}

void test_alarm_service__delete_passes_through(void) {
  s_delete_result = AlarmServiceResultErrDoesNotExist;

  cl_assert_equal_i(alarm_service_delete(4), AlarmServiceResultErrDoesNotExist);
  cl_assert_equal_i(s_delete_calls, 1);
  cl_assert_equal_i(s_delete_id, 4);
}
