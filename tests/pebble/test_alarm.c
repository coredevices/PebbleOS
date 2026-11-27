/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_alarm_common.h"

#include <errno.h>

// Stubs
#include "fake_pfs.h"

#include <pbl/kernel/time.h>
#include <pbl/util/units.h>

#include <fake_work.h>

// The production alarm slot count; the test build uses a smaller fake pin array.
#define TEST_MAX_ALARMS 10

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Counter variables
static int s_alarm_timer_timeout_ms = 0;
static bool s_activity_tracking_on = true;
static ActivitySleepState s_sleep_state = ActivitySleepStateUnknown;
static int32_t s_vmc;

bool activity_tracking_on(void) {
  return s_activity_tracking_on;
}

bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  cl_assert_equal_i(history_len, 1);
  if (metric == ActivityMetricSleepState) {
    *history = s_sleep_state;
    return true;
  }
  if (metric == ActivityMetricLastVMC) {
    *history = s_vmc;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Fakes

time_t pbl_realtime(void) {
  return s_current_day + prv_hours_and_minutes_to_seconds(s_current_hour, s_current_minute);
}

uint64_t pbl_realtime_ms(void) {
  return (uint64_t)pbl_realtime() * 1000U;
}

uint64_t pbl_uptime_ticks(void) {
  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Setup

void test_alarm__initialize(void) {
  fake_work_cleanup();
  s_num_timeline_adds = 0;
  s_num_timeline_removes = 0;
  s_num_alarm_events_put = 0;
  s_alarm_timer_timeout_ms = 0;
  s_num_alarms_fired = 0;
  s_activity_tracking_on = true;
  s_sleep_state = ActivitySleepStateUnknown;
  s_vmc = 0;
  alarm_test_set_persist_failure(false);
  alarm_test_set_delete_failure(false);
  s_defer_sys_work_callbacks = false;
  s_pending_sys_work_callback = nullptr;
  s_pending_sys_work_data = nullptr;

  s_current_hour = 0;
  s_current_minute = 0;
  // Default to Thursday
  s_current_day = s_thursday;

  timeline_item_destroy(s_last_timeline_item_added);
  s_last_timeline_item_added = nullptr;
  s_last_timeline_item_removed_uuid = (Uuid){};
  memset(s_fake_pin_records, 0, sizeof(s_fake_pin_records));

  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");

  pbl_cron_init();

  alarm_init();
  alarm_service_enable_alarms(true);
}

void test_alarm__cleanup(void) {
  pbl_cron_deinit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Basic Store / Get Tests

void test_alarm__alarm_create_recurring_daily(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 5, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 5, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 6, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
}

void test_alarm__can_schedule(void) {
  cl_assert(alarm_can_schedule());

  int count = 0;
  while (alarm_can_schedule() && count < 20) {
    AlarmId id =
        alarm_create(&(AlarmInfo){.hour = 3 + count, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
    cl_assert(id != ALARM_INVALID_ID);
    count++;
  }

  cl_assert_equal_i(count, 10);
  cl_assert(!alarm_can_schedule());

  alarm_delete(0);
  cl_assert(alarm_can_schedule());
}

typedef struct {
  AlarmId ids[TEST_MAX_ALARMS];
  size_t count;
} CollectedAlarmIds;

static void prv_collect_alarm_id(AlarmId id, const AlarmInfo *info, void *context) {
  (void)info;
  CollectedAlarmIds *collected = context;
  if (collected->count < TEST_MAX_ALARMS) {
    collected->ids[collected->count] = id;
  }
  collected->count++;
}

void test_alarm__for_each(void) {
  CollectedAlarmIds collected = {};
  alarm_for_each(prv_collect_alarm_id, &collected);
  cl_assert_equal_i(collected.count, 0);

  const AlarmId id0 =
      alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  const AlarmId id1 =
      alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  const AlarmId id2 =
      alarm_create(&(AlarmInfo){.hour = 5, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  cl_assert(id0 != ALARM_INVALID_ID);
  cl_assert(id1 != ALARM_INVALID_ID);
  cl_assert(id2 != ALARM_INVALID_ID);

  // Disabled alarms are still configured and listed.
  alarm_set_enabled(id1, false);

  collected = (CollectedAlarmIds){};
  alarm_for_each(prv_collect_alarm_id, &collected);
  cl_assert_equal_i(collected.count, 3);
  cl_assert(collected.ids[0] == id0 || collected.ids[0] == id1 || collected.ids[0] == id2);
  cl_assert(collected.ids[1] == id0 || collected.ids[1] == id1 || collected.ids[1] == id2);
  cl_assert(collected.ids[2] == id0 || collected.ids[2] == id1 || collected.ids[2] == id2);
  cl_assert(collected.ids[0] != collected.ids[1] && collected.ids[0] != collected.ids[2] &&
            collected.ids[1] != collected.ids[2]);

  alarm_delete(id1);
  collected = (CollectedAlarmIds){};
  alarm_for_each(prv_collect_alarm_id, &collected);
  cl_assert_equal_i(collected.count, 2);
  cl_assert(collected.ids[0] == id0 || collected.ids[0] == id2);
  cl_assert(collected.ids[1] == id0 || collected.ids[1] == id2);
  cl_assert(collected.ids[0] != collected.ids[1]);
}

void test_alarm__set_enabled_checked(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  cl_assert(alarm_get_enabled(id));

  cl_assert_equal_i(alarm_set_enabled_checked(id, false), AlarmSetEnabledResultOk);
  cl_assert(!alarm_get_enabled(id));

  cl_assert_equal_i(alarm_set_enabled_checked(id, true), AlarmSetEnabledResultOk);
  cl_assert(alarm_get_enabled(id));

  cl_assert_equal_i(alarm_set_enabled_checked(9, true), AlarmSetEnabledResultDoesNotExist);
}

void test_alarm__out_of_range_id_is_rejected(void) {
  const AlarmId id = TEST_MAX_ALARMS;
  AlarmStorageKey key = {.id = id, .type = ALARM_DATA_CONFIG};
  uint8_t config[64] = {0};
  config[2] = 7;  // hour
  config[3] = 30; // minute

  struct pbl_settings_file file;
  cl_must_pass(pbl_settings_open(&file, "/alarms", 1024));
  cl_must_pass(pbl_settings_set(&file, &key, sizeof(key), config, sizeof(config)));
  pbl_settings_close(&file);

  cl_assert_equal_i(alarm_set_enabled_checked(id, false), AlarmSetEnabledResultDoesNotExist);

  AlarmInfo info;
  bool scheduled_days[PBL_DAY_PER_WEEK];
  cl_assert_equal_i(alarm_get_info_checked(id, &info, scheduled_days), AlarmGetResultDoesNotExist);
}

void test_alarm__smart_alarm_created_without_tracking(void) {
  s_activity_tracking_on = false;

  AlarmId id = ALARM_INVALID_ID;
  cl_assert_equal_i(
      alarm_create_checked(
          &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true},
          &id),
      AlarmCreateResultOk);
  cl_assert(id != ALARM_INVALID_ID);

  // Without tracking, the smart alarm fires at its configured time.
  time_t time_until;
  cl_assert(alarm_get_time_until(id, &time_until));
  cl_assert_equal_i(time_until, 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  // Starting tracking adds the early window.
  s_activity_tracking_on = true;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);
  cl_assert(alarm_get_time_until(id, &time_until));
  cl_assert_equal_i(time_until, 10 * PBL_SEC_PER_HOUR);

  // Losing tracking after creation removes the early window when alarms are reloaded.
  s_activity_tracking_on = false;
  alarm_handle_clock_change();

  cl_assert(alarm_get_time_until(id, &time_until));
  cl_assert_equal_i(time_until, 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);
}

void test_alarm__get_info_checked_includes_custom_days(void) {
  bool scheduled_days[PBL_DAY_PER_WEEK] = {true, true, false, false, false, false, false};
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &scheduled_days,
    .is_smart = false,
    .sound_enabled = true,
    .vibrate_enabled = false,
    .tone = AlarmTone_Bell,
  });

  AlarmInfo info;
  bool days[PBL_DAY_PER_WEEK];
  cl_assert_equal_i(alarm_get_info_checked(id, &info, days), AlarmGetResultOk);
  cl_assert_equal_i(info.hour, 10);
  cl_assert_equal_i(info.minute, 30);
  cl_assert_equal_i(info.kind, ALARM_KIND_CUSTOM);
  cl_assert(info.sound_enabled);
  cl_assert(!info.vibrate_enabled);
  cl_assert_equal_i(info.tone, AlarmTone_Bell);
  cl_assert(days[0]);
  cl_assert(days[1]);
  cl_assert(!days[2]);

  cl_assert_equal_i(alarm_get_info_checked(9, &info, days), AlarmGetResultDoesNotExist);
}

void test_alarm__smart_alarm_fires_at_time_without_tracking(void) {
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  s_activity_tracking_on = false;
  alarm_handle_clock_change();
  s_sleep_state = ActivitySleepStateLightSleep;

  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert(!fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__enable_enabled_alarm_is_noop(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  const int timeline_adds = s_num_timeline_adds;

  cl_assert_equal_i(alarm_set_enabled_checked(id, true), AlarmSetEnabledResultOk);
  cl_assert_equal_i(s_num_timeline_adds, timeline_adds);
}

void test_alarm__failed_disable_keeps_snooze(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  alarm_set_snooze_alarm();
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));

  alarm_test_set_persist_failure(true);
  cl_assert_equal_i(alarm_set_enabled_checked(id, false), AlarmSetEnabledResultStorageError);
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__noop_disable_cancels_snooze(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  alarm_set_snooze_alarm();
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));

  // The one-shot alarm was auto-disabled when it fired, so this is a no-op
  // configuration change that must still cancel the snooze.
  cl_assert_equal_i(alarm_set_enabled_checked(id, false), AlarmSetEnabledResultOk);
  cl_assert(!fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__failed_just_once_disable_keeps_later_alarm(void) {
  AlarmId id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 10, .kind = ALARM_KIND_JUST_ONCE});
  AlarmId id2 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 20, .kind = ALARM_KIND_JUST_ONCE});

  s_current_hour = 10;
  s_current_minute = 10;
  alarm_test_set_persist_failure(true);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  AlarmInfo info;
  cl_assert(alarm_get_info(id1, &info));
  cl_assert(info.enabled);

  s_current_minute = 20;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  cl_assert(alarm_get_info(id2, &info));
}

void test_alarm__create_checked_no_space(void) {
  int count = 0;
  while (alarm_can_schedule() && count < 20) {
    AlarmId id = ALARM_INVALID_ID;
    cl_assert_equal_i(
        alarm_create_checked(
            &(AlarmInfo){.hour = 3 + count, .minute = 14, .kind = ALARM_KIND_EVERYDAY}, &id),
        AlarmCreateResultOk);
    cl_assert(id != ALARM_INVALID_ID);
    count++;
  }

  cl_assert_equal_i(count, 10);

  AlarmId id = 42;
  cl_assert_equal_i(alarm_create_checked(
                        &(AlarmInfo){.hour = 13, .minute = 14, .kind = ALARM_KIND_EVERYDAY}, &id),
                    AlarmCreateResultNoSpace);
  cl_assert_equal_i(id, ALARM_INVALID_ID);

  // The legacy creator must fail cleanly on the same full store.
  cl_assert_equal_i(
      alarm_create(&(AlarmInfo){.hour = 13, .minute = 14, .kind = ALARM_KIND_EVERYDAY}),
      ALARM_INVALID_ID);
}

void test_alarm__create_checked_invalid(void) {
  AlarmId id = 42;
  cl_assert_equal_i(alarm_create_checked(NULL, &id), AlarmCreateResultInvalidArgument);
  cl_assert_equal_i(id, ALARM_INVALID_ID);

  id = 42;
  cl_assert_equal_i(alarm_create_checked(
                        &(AlarmInfo){.hour = 24, .minute = 14, .kind = ALARM_KIND_EVERYDAY}, &id),
                    AlarmCreateResultInvalidArgument);
  cl_assert_equal_i(id, ALARM_INVALID_ID);

  id = 42;
  cl_assert_equal_i(
      alarm_create_checked(&(AlarmInfo){.hour = 3, .minute = 60, .kind = ALARM_KIND_EVERYDAY}, &id),
      AlarmCreateResultInvalidArgument);
  cl_assert_equal_i(id, ALARM_INVALID_ID);

  cl_assert_equal_i(alarm_create_checked(
                        &(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY}, NULL),
                    AlarmCreateResultInvalidArgument);
}

void test_alarm__create_checked_storage_failure(void) {
  struct pbl_settings_file file;
  cl_must_pass(pbl_settings_open(&file, "/alarms", 1024));

  uint8_t filler[100] = {};
  int rv = 0;
  for (int i = 0; i < 32; ++i) {
    rv = pbl_settings_set(&file, &i, sizeof(i), filler, sizeof(filler));
    if (rv < 0) {
      break;
    }
  }
  // Fill the remaining tail with small records so the alarm configuration cannot fit.
  for (int i = 0; i < UINT8_MAX; ++i) {
    const uint8_t key[2] = {0xff, i};
    const uint8_t value = 0;
    rv = pbl_settings_set(&file, key, sizeof(key), &value, sizeof(value));
    if (rv < 0) {
      break;
    }
  }
  pbl_settings_close(&file);
  cl_assert(rv < 0);

  AlarmId id = 42;
  cl_assert_equal_i(
      alarm_create_checked(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY}, &id),
      AlarmCreateResultStorageError);
  cl_assert_equal_i(id, ALARM_INVALID_ID);

  AlarmInfo info;
  cl_assert(!alarm_get_info(0, &info));
}

void test_alarm__update_checked(void) {
  bool days[PBL_DAY_PER_WEEK] = {false, true, true, false, true, false, false};
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  cl_assert(id != ALARM_INVALID_ID);
  alarm_set_enabled(id, false);

  const AlarmInfo update = {
    .hour = 7,
    .minute = 15,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &days,
    .enabled = true,
    .is_smart = true,
    .sound_enabled = true,
    .vibrate_enabled = true,
    .tone = AlarmTone_Chime,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);

  AlarmInfo info;
  bool stored_days[PBL_DAY_PER_WEEK];
  cl_assert_equal_i(alarm_get_info_checked(id, &info, stored_days), AlarmGetResultOk);
  cl_assert_equal_i(info.hour, 7);
  cl_assert_equal_i(info.minute, 15);
  cl_assert_equal_i(info.kind, ALARM_KIND_CUSTOM);
  cl_assert_equal_i(memcmp(stored_days, days, sizeof(days)), 0);
  cl_assert(info.is_smart);
  cl_assert(info.sound_enabled);
  cl_assert(info.vibrate_enabled);
  cl_assert_equal_i(info.tone, AlarmTone_Chime);
  cl_assert(!info.enabled);

  // A built-in recurrence replaces the custom days and the other settings.
  const AlarmInfo weekdays = {
    .hour = 6,
    .minute = 45,
    .kind = ALARM_KIND_WEEKDAYS,
  };
  cl_assert_equal_i(alarm_update_checked(id, &weekdays), AlarmUpdateResultOk);
  cl_assert_equal_i(alarm_get_info_checked(id, &info, stored_days), AlarmGetResultOk);
  cl_assert_equal_i(info.kind, ALARM_KIND_WEEKDAYS);
  const bool expected_days[PBL_DAY_PER_WEEK] = {false, true, true, true, true, true, false};
  cl_assert_equal_i(memcmp(stored_days, expected_days, sizeof(expected_days)), 0);
  cl_assert(!info.is_smart);
  cl_assert(!info.sound_enabled);
  cl_assert(!info.vibrate_enabled);
  cl_assert_equal_i(info.tone, AlarmTone_Reveille);
  cl_assert(!info.enabled);
}

void test_alarm__update_checked_errors(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  const AlarmInfo valid = {
    .hour = 7,
    .minute = 15,
    .kind = ALARM_KIND_EVERYDAY,
  };

  AlarmInfo bad = valid;
  bad.hour = 24;
  cl_assert_equal_i(alarm_update_checked(id, &bad), AlarmUpdateResultInvalidArgument);

  bad = valid;
  bad.kind = ALARM_KIND_CUSTOM;
  bad.scheduled_days = NULL;
  cl_assert_equal_i(alarm_update_checked(id, &bad), AlarmUpdateResultInvalidArgument);

  bad = valid;
  bad.tone = (AlarmTone)99;
  cl_assert_equal_i(alarm_update_checked(id, &bad), AlarmUpdateResultInvalidArgument);

  cl_assert_equal_i(alarm_update_checked(id, NULL), AlarmUpdateResultInvalidArgument);
  cl_assert_equal_i(alarm_update_checked(TEST_MAX_ALARMS, &valid), AlarmUpdateResultDoesNotExist);
  const AlarmId unused = (id == 0) ? 9 : 0;
  cl_assert_equal_i(alarm_update_checked(unused, &valid), AlarmUpdateResultDoesNotExist);

  alarm_test_set_persist_failure(true);
  cl_assert_equal_i(alarm_update_checked(id, &valid), AlarmUpdateResultStorageError);
  alarm_test_set_persist_failure(false);

  AlarmInfo info;
  cl_assert(alarm_get_info(id, &info));
  cl_assert_equal_i(info.hour, 10);
  cl_assert_equal_i(info.minute, 30);
}

void test_alarm__update_tone_after_smart_fire_does_not_refire(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  s_sleep_state = ActivitySleepStateUnknown;

  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // The smart alarm fires at 06:30 for the 07:00 occurrence.
  s_current_hour = 6;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // A tone-only update must not disarm the fired-occurrence guard.
  s_current_minute = 35;
  const AlarmInfo update = {
    .hour = 7,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
    .tone = AlarmTone_Bell,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);

  s_current_hour = 7;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__update_tone_keeps_snooze(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  alarm_set_snooze_alarm();
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));

  const AlarmInfo update = {
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .tone = AlarmTone_Bell,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__update_schedule_change_cancels_snooze(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  alarm_set_snooze_alarm();
  cl_assert(fake_work_is_scheduled(&s_snooze_timer));

  const AlarmInfo update = {
    .hour = 11,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);
  cl_assert(!fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__delete_checked(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  cl_assert(id != ALARM_INVALID_ID);

  cl_assert_equal_i(alarm_delete_checked(id), AlarmDeleteResultOk);
  prv_assert_alarm_config_absent(id);
  assert_alarm_pins_absent(id);

  cl_assert_equal_i(alarm_delete_checked(id), AlarmDeleteResultDoesNotExist);
  cl_assert_equal_i(alarm_delete_checked(TEST_MAX_ALARMS), AlarmDeleteResultDoesNotExist);
}

void test_alarm__delete_checked_storage_failure(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  cl_assert(id != ALARM_INVALID_ID);

  alarm_test_set_delete_failure(true);
  cl_assert_equal_i(alarm_delete_checked(id), AlarmDeleteResultStorageError);
  cl_assert(alarm_get_enabled(id));

  alarm_delete(id);
  prv_assert_alarm_config_absent(id);
}

void test_alarm__alarm_create_recurring_weekends(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 5, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 5, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 6, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
}

void test_alarm__alarm_create_recurring_weekdays(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 5, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 5, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 6, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
}

void test_alarm__alarm_create_just_once(void) {
  AlarmId id;
  // It's currently Thursday @ 00:00
  bool just_once_schedule_thursday[7] = {false, false, false, false, true, false, false};
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  id = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  id = alarm_create(&(AlarmInfo){.hour = 5, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id, 5, 14, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id, 6, 14, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
}

void test_alarm__alarm_create_recurring_custom(void) {
  AlarmId id;
  bool custom_schedule1[7] = {true, false, true, false, false, true, true};
  id = alarm_create(&(AlarmInfo){
    .hour = 3,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule1
  });
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_CUSTOM, custom_schedule1);

  bool custom_schedule2[7] = {false, false, false, false, false, true, false};
  id = alarm_create(&(AlarmInfo){
    .hour = 4,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule2
  });
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_CUSTOM, custom_schedule2);

  bool custom_schedule3[7] = {true, true, true, true, true, true, true};
  id = alarm_create(&(AlarmInfo){
    .hour = 5,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule3
  });
  prv_assert_alarm_config(id, 5, 14, false, ALARM_KIND_CUSTOM, custom_schedule3);

  // FIXME:
  bool custom_schedule4[7] = {false, false, false, false, false, false, false};
  id = alarm_create(&(AlarmInfo){
    .hour = 6,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule4
  });
  prv_assert_alarm_config(id, 6, 14, false, ALARM_KIND_CUSTOM, custom_schedule4);
}

void test_alarm__alarm_set_time(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id1, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  alarm_set_time(id1, 5, 6);
  prv_assert_alarm_config(id1, 5, 6, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  id2 = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id2, 4, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  alarm_set_time(id2, 23, 56);
  prv_assert_alarm_config(id2, 23, 56, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);

  alarm_set_time(id1, 15, 16);
  prv_assert_alarm_config(id1, 15, 16, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  alarm_set_time(id2, 23, 46);
  prv_assert_alarm_config(id2, 23, 46, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
}

void test_alarm__alarm_set_recurring_daily(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  alarm_set_kind(id, ALARM_KIND_EVERYDAY);
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  bool custom_schedule1[7] = {true, false, true, false, false, true, true};
  id = alarm_create(&(AlarmInfo){
    .hour = 4,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule1
  });
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_CUSTOM, custom_schedule1);
  alarm_set_kind(id, ALARM_KIND_EVERYDAY);
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
}

void test_alarm__alarm_set_recurring_weekends(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  alarm_set_kind(id, ALARM_KIND_WEEKENDS);
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);

  bool custom_schedule1[7] = {true, false, true, false, false, true, true};
  id = alarm_create(&(AlarmInfo){
    .hour = 4,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule1
  });
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_CUSTOM, custom_schedule1);
  alarm_set_kind(id, ALARM_KIND_WEEKENDS);
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
}

void test_alarm__alarm_set_recurring_weekdays(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  alarm_set_kind(id, ALARM_KIND_WEEKDAYS);
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);

  bool custom_schedule1[7] = {true, false, true, false, false, true, true};
  id = alarm_create(&(AlarmInfo){
    .hour = 4,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &custom_schedule1
  });
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_CUSTOM, custom_schedule1);
  alarm_set_kind(id, ALARM_KIND_WEEKDAYS);
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
}

void test_alarm__alarm_set_custom(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  bool custom_schedule1[7] = {true, false, true, false, false, true, true};
  alarm_set_custom(id, custom_schedule1);
  prv_assert_alarm_config(id, 3, 14, false, ALARM_KIND_CUSTOM, custom_schedule1);

  id = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  bool custom_schedule2[7] = {true, false, false, false, false, true, false};
  alarm_set_custom(id, custom_schedule2);
  prv_assert_alarm_config(id, 4, 14, false, ALARM_KIND_CUSTOM, custom_schedule2);
}

void test_alarm__alarm_get_custom_days(void) {
  AlarmId id1, id2;

  bool schedule_1[7] = {true, false, false, false, false, false, true};
  bool verify_schedule_1[7] = {false, false, false, false, false, false, false};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 3,
    .minute = 14,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 3, 14, false, ALARM_KIND_CUSTOM, schedule_1);
  alarm_get_custom_days(id1, verify_schedule_1);
  prv_assert_alarm_config(id1, 3, 14, false, ALARM_KIND_CUSTOM, verify_schedule_1);

  bool schedule_2[7] = {false, false, true, false, false, true, false};
  bool verify_schedule_2[7] = {false, false, false, false, false, false, false};
  id2 = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  alarm_set_custom(id2, schedule_2);
  prv_assert_alarm_config(id2, 4, 14, false, ALARM_KIND_CUSTOM, schedule_2);
  alarm_get_custom_days(id2, verify_schedule_2);
  prv_assert_alarm_config(id2, 4, 14, false, ALARM_KIND_CUSTOM, verify_schedule_2);
}

void test_alarm__alarm_get_string_for_custom(void) {
  bool schedule_1[7] = {true, false, false, false, false, false, true};
  char alarm_day_text_1[32] = {};
  alarm_get_string_for_custom(schedule_1, alarm_day_text_1);
  cl_assert_equal_s(alarm_day_text_1, "Sat,Sun");

  bool schedule_2[7] = {true, true, true, true, true, true, true};
  char alarm_day_text_2[32] = {};
  alarm_get_string_for_custom(schedule_2, alarm_day_text_2);
  cl_assert_equal_s(alarm_day_text_2, "Mon,Tue,Wed,Thu,Fri,Sat,Sun");

  bool schedule_3[7] = {false, true, false, false, false, false, false};
  char alarm_day_text_3[32] = {};
  alarm_get_string_for_custom(schedule_3, alarm_day_text_3);
  cl_assert_equal_s(alarm_day_text_3, "Mondays");

  bool schedule_4[7] = {false, false, true, false, false, false, false};
  char alarm_day_text_4[32] = {};
  alarm_get_string_for_custom(schedule_4, alarm_day_text_4);
  cl_assert_equal_s(alarm_day_text_4, "Tuesdays");

  bool schedule_5[7] = {false, false, false, true, false, false, false};
  char alarm_day_text_5[32] = {};
  alarm_get_string_for_custom(schedule_5, alarm_day_text_5);
  cl_assert_equal_s(alarm_day_text_5, "Wednesdays");

  bool schedule_6[7] = {false, false, false, false, true, false, false};
  char alarm_day_text_6[32] = {};
  alarm_get_string_for_custom(schedule_6, alarm_day_text_6);
  cl_assert_equal_s(alarm_day_text_6, "Thursdays");

  bool schedule_7[7] = {false, false, false, false, false, true, false};
  char alarm_day_text_7[32] = {};
  alarm_get_string_for_custom(schedule_7, alarm_day_text_7);
  cl_assert_equal_s(alarm_day_text_7, "Fridays");

  bool schedule_8[7] = {false, false, false, false, false, false, true};
  char alarm_day_text_8[32] = {};
  alarm_get_string_for_custom(schedule_8, alarm_day_text_8);
  cl_assert_equal_s(alarm_day_text_8, "Saturdays");

  bool schedule_9[7] = {true, false, false, false, false, false, false};
  char alarm_day_text_9[32] = {};
  alarm_get_string_for_custom(schedule_9, alarm_day_text_9);
  cl_assert_equal_s(alarm_day_text_9, "Sundays");
}

void test_alarm__alarm_set_get_enabled(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  alarm_set_enabled(id1, false);
  prv_assert_alarm_config(id1, 3, 14, true, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 3);

  alarm_set_enabled(id1, true);
  prv_assert_alarm_config(id1, 3, 14, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 3);

  id2 = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  cl_assert_equal_i(s_num_timeline_adds, 9);
  cl_assert_equal_i(s_num_timeline_removes, 3);

  alarm_set_enabled(id2, false);
  prv_assert_alarm_config(id2, 4, 14, true, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 12);
  cl_assert_equal_i(s_num_timeline_removes, 9);

  AlarmId invalid_id = 7;
  alarm_set_enabled(invalid_id, false);
  cl_assert_equal_i(s_num_timeline_adds, 12);
  cl_assert_equal_i(s_num_timeline_removes, 9);
}

void test_alarm__alarm_delete(void) {
  AlarmId id1, id2, id4;
  id1 = alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  id2 = alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  alarm_delete(id1);
  prv_assert_alarm_config_absent(id1);
  assert_alarm_pins_absent(id1);
  alarm_delete(id2);
  prv_assert_alarm_config_absent(id2);
  assert_alarm_pins_absent(id2);
  alarm_create(&(AlarmInfo){.hour = 13, .minute = 13, .kind = ALARM_KIND_WEEKDAYS});
  id4 = alarm_create(&(AlarmInfo){.hour = 14, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});
  alarm_create(&(AlarmInfo){.hour = 15, .minute = 15, .kind = ALARM_KIND_WEEKDAYS});
  alarm_create(&(AlarmInfo){.hour = 16, .minute = 16, .kind = ALARM_KIND_WEEKDAYS});
  alarm_delete(id4);
  prv_assert_alarm_config_absent(id4);
  assert_alarm_pins_absent(id4);
  alarm_create(&(AlarmInfo){.hour = 17, .minute = 17, .kind = ALARM_KIND_WEEKENDS});

  for (int i = 0; i < MAX_CONFIGURED_ALARMS; ++i) {
    alarm_delete(i);
    prv_assert_alarm_config_absent(i);
    assert_alarm_pins_absent(i);
  }

  alarm_delete(1);
}

void test_alarm__snooze_delay(void) {
  int delay;
  delay = alarm_get_snooze_delay();
  cl_assert_equal_i(delay, 10);
  alarm_set_snooze_delay(15);

  delay = alarm_get_snooze_delay();
  cl_assert_equal_i(delay, 15);
}

void test_alarm__set_snooze_alarm() {
  alarm_set_snooze_alarm();
  cl_assert_equal_i(fake_work_timeout_ms(&s_snooze_timer), 10 * 1000);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
  s_current_minute = 10;
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm__get_string_for_kind(void) {
  bool all_caps = false;
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_EVERYDAY, all_caps), "Every Day");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_WEEKDAYS, all_caps), "Weekdays");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_WEEKENDS, all_caps), "Weekends");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_JUST_ONCE, all_caps), "Once");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_CUSTOM, all_caps), "Custom");

  all_caps = true;
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_EVERYDAY, all_caps), "EVERY DAY");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_WEEKDAYS, all_caps), "WEEKDAYS");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_WEEKENDS, all_caps), "WEEKENDS");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_JUST_ONCE, all_caps), "ONCE");
  cl_assert_equal_s(alarm_get_string_for_kind(ALARM_KIND_CUSTOM, all_caps), "CUSTOM");
}

void test_alarm__handle_clock_change(void) {
  alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_WEEKENDS});
  alarm_create(&(AlarmInfo){.hour = 13, .minute = 14, .kind = ALARM_KIND_WEEKDAYS});

  s_current_hour = 12;
  s_current_minute = 14;
  alarm_handle_clock_change();

  s_current_hour = 13;
  s_current_minute = 14;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Alarm Timeline Pin Tests

void test_alarm__pin_add(void) {
  const AlarmId dummy_alarm_id = 0;
  const AlarmKind alarm_kind = ALARM_KIND_WEEKENDS;
  Uuid added_pin_uuid;
  alarm_pin_add(s_monday, dummy_alarm_id, AlarmType_Basic, alarm_kind, &added_pin_uuid);
  cl_assert(uuid_equal(&added_pin_uuid, &s_last_timeline_item_added->header.id));

  AttributeList *pin_attr_list = &s_last_timeline_item_added->attr_list;

  const uint32_t pin_icon_tiny = attribute_get_uint32(pin_attr_list, AttributeIdIconTiny, 0);
  cl_assert_equal_i((int)pin_icon_tiny, TIMELINE_RESOURCE_ALARM_CLOCK);

  const char *pin_title = attribute_get_string(pin_attr_list, AttributeIdTitle, nullptr);
  cl_assert_equal_s(pin_title, "Alarm");

  const char *pin_subtitle = attribute_get_string(pin_attr_list, AttributeIdSubtitle, nullptr);
  cl_assert_equal_s(pin_subtitle, alarm_get_string_for_kind(alarm_kind, false /* all_caps */));

  const AlarmKind pin_alarm_kind =
      (AlarmKind)attribute_get_uint8(pin_attr_list, AttributeIdAlarmKind, 0);
  cl_assert_equal_i(pin_alarm_kind, alarm_kind);

  cl_assert_equal_i(s_last_timeline_item_added->action_group.num_actions, 1);

  const TimelineItemAction *alarm_action = s_last_timeline_item_added->action_group.actions;
  cl_assert_equal_i(alarm_action->id, dummy_alarm_id);
  cl_assert_equal_i(alarm_action->type, TimelineItemActionTypeOpenWatchApp);

  const AttributeList *action_attr_list = &alarm_action->attr_list;

  const char *action_title = attribute_get_string(action_attr_list, AttributeIdTitle, nullptr);
  cl_assert_equal_s(action_title, "Edit");
}

void test_alarm__language_change_rebuilds_pins(void) {
  alarm_create(&(AlarmInfo){.hour = 3, .minute = 14, .kind = ALARM_KIND_EVERYDAY});
  alarm_create(&(AlarmInfo){.hour = 4, .minute = 14, .kind = ALARM_KIND_JUST_ONCE});
  const int num_pins = s_num_timeline_adds;
  cl_assert(num_pins > 0);

  cl_assert(s_language_change_event_info);
  PebbleEvent event = {.type = PEBBLE_LANGUAGE_CHANGE_EVENT};
  s_language_change_event_info->handler(&event, s_language_change_event_info->context);
  cl_assert_equal_i(s_num_timeline_removes, num_pins);
  cl_assert_equal_i(s_num_timeline_adds, 2 * num_pins);
}

void test_alarm__pin_remove(void) {
  const AlarmId dummy_alarm_id = 0;
  Uuid pin_uuid;
  alarm_pin_add(s_monday, dummy_alarm_id, AlarmType_Basic, ALARM_KIND_WEEKENDS, &pin_uuid);
  alarm_pin_remove(&pin_uuid);
  cl_assert(uuid_equal(&pin_uuid, &s_last_timeline_item_removed_uuid));
}

void test_alarm__reload_removes_untracked_future_pins(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 7, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  AlarmStorageKey key = {.id = id, .type = ALARM_DATA_PINS};
  Uuid tracked[3];
  struct pbl_settings_file file;
  cl_must_pass(pbl_settings_open(&file, "/alarms", 1024));
  cl_must_pass(pbl_settings_get(&file, &key, sizeof(key), tracked, sizeof(tracked)));
  pbl_settings_close(&file);

  const Uuid alarm_source = UUID_ALARMS_DATA_SOURCE;
  const Uuid other_source = UUID_REMINDERS_DATA_SOURCE;
  const Uuid orphan = {.byte0 = 1};
  const Uuid past = {.byte0 = 2};
  const Uuid unrelated = {.byte0 = 3};
  prv_fake_pin_record_add(orphan, alarm_source, pbl_realtime() + PBL_SEC_PER_HOUR, LayoutIdAlarm);
  prv_fake_pin_record_add(past, alarm_source, pbl_realtime() - PBL_SEC_PER_HOUR, LayoutIdAlarm);
  prv_fake_pin_record_add(unrelated, other_source, pbl_realtime() + PBL_SEC_PER_HOUR,
                          LayoutIdAlarm);
  prv_fake_pin_record_add(tracked[0], alarm_source, pbl_realtime() + PBL_SEC_PER_HOUR,
                          LayoutIdAlarm);

  const int removes_before = s_num_timeline_removes;
  alarm_handle_clock_change();

  cl_assert(!s_fake_pin_records[0].exists);
  cl_assert(s_fake_pin_records[1].exists);
  cl_assert(s_fake_pin_records[2].exists);
  cl_assert(!s_fake_pin_records[3].exists);
  cl_assert_equal_i(s_num_timeline_removes - removes_before, 4);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! More Advanced Tests

void test_alarm__recurring_daily_alarm_timeout_ahead(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);

  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__recurring_daily_alarm_timeout_behind(void) {
  AlarmId id;
  s_current_hour = 11;
  s_current_minute = 30;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);

  // Alarm set for tomorrow, so add 24 hours.
  s_current_hour = 10 + 24;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__recurring_daily_alarm(void) {
  time_t next_alarm_time = 0;
  alarm_get_next_enabled_alarm(&next_alarm_time);
  cl_assert_equal_i(next_alarm_time, 0);

  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);

  alarm_get_next_enabled_alarm(&next_alarm_time);
  cl_assert_equal_i(next_alarm_time, s_current_day + 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  alarm_get_next_enabled_alarm(&next_alarm_time);
  cl_assert_equal_i(next_alarm_time, s_current_day + 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 13);
  cl_assert_equal_i(s_num_timeline_removes, 6);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(s_num_timeline_adds, 20);
  cl_assert_equal_i(s_num_timeline_removes, 12);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_friday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);
  cl_assert_equal_i(s_num_alarm_events_put, 3);
  cl_assert_equal_i(s_num_timeline_adds, 27);
  cl_assert_equal_i(s_num_timeline_removes, 18);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 4);
  cl_assert_equal_i(s_num_alarm_events_put, 4);
  cl_assert_equal_i(s_num_timeline_adds, 34);
  cl_assert_equal_i(s_num_timeline_removes, 24);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 5);
  cl_assert_equal_i(s_num_alarm_events_put, 5);
  cl_assert_equal_i(s_num_timeline_adds, 41);
  cl_assert_equal_i(s_num_timeline_removes, 30);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 6);
  cl_assert_equal_i(s_num_alarm_events_put, 6);
  cl_assert_equal_i(s_num_timeline_adds, 48);
  cl_assert_equal_i(s_num_timeline_removes, 36);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_sunday; // Make sure the wday can wrap properly
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 7);
  cl_assert_equal_i(s_num_alarm_events_put, 7);
  cl_assert_equal_i(s_num_timeline_adds, 55);
  cl_assert_equal_i(s_num_timeline_removes, 42);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());
}

void test_alarm__recurring_weekends_alarm_timeout_ahead(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);

  // It's currently Thursday @ 00:00. The alarm shouldn't go off for over 2 days + 10:30
  s_current_hour = 10;
  s_current_minute = 29;
  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
  // Only 1 pin should be added (for Saturday)
  cl_assert_equal_i(s_num_timeline_adds, 1);

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_timeline_adds, 3);
}

void test_alarm__recurring_weekends_alarm_timeout_behind(void) {
  AlarmId id;
  s_current_hour = 11;
  s_current_minute = 30;
  s_current_day = s_saturday;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  // Only 1 pin should be added (for Sunday)
  cl_assert_equal_i(s_num_timeline_adds, 1);
}

void test_alarm__recurring_weekends_alarm(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  // Only 1 pin should be added (for Saturday)
  cl_assert_equal_i(s_num_timeline_adds, 1);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_WEEKENDS});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_WEEKENDS, s_weekend_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 2);

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 2);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(s_num_timeline_adds, 9);
  cl_assert_equal_i(s_num_timeline_removes, 5);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_sunday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);
  cl_assert_equal_i(s_num_alarm_events_put, 3);
  cl_assert_equal_i(s_num_timeline_adds, 11);
  cl_assert_equal_i(s_num_timeline_removes, 7);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again, but not until Saturday
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 4);
  cl_assert_equal_i(s_num_alarm_events_put, 4);
  cl_assert_equal_i(s_num_timeline_adds, 12);
  cl_assert_equal_i(s_num_timeline_removes, 8);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());
}

void test_alarm__recurring_weekday_alarm_timeout_ahead(void) {
  AlarmId id;
  s_current_day = s_saturday;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  // Only 1 pin should be added (for Monday)
  cl_assert_equal_i(s_num_timeline_adds, 1);
}

void test_alarm__recurring_weekday_alarm_timeout_behind(void) {
  AlarmId id;
  s_current_hour = 11;
  s_current_minute = 30;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  // Only 1 pin should be added (for Friday)
  cl_assert_equal_i(s_num_timeline_adds, 1);
}

void test_alarm__recurring_weekday_alarm(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  // 2 pins should be added (for Thursday / Friday)
  cl_assert_equal_i(s_num_timeline_adds, 2);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 4);

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 8);
  cl_assert_equal_i(s_num_timeline_removes, 4);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(s_num_timeline_adds, 11);
  cl_assert_equal_i(s_num_timeline_removes, 7);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_friday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);
  cl_assert_equal_i(s_num_alarm_events_put, 3);
  cl_assert_equal_i(s_num_timeline_adds, 14);
  cl_assert_equal_i(s_num_timeline_removes, 9);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again, but not until Monday
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 4);
  cl_assert_equal_i(s_num_alarm_events_put, 4);
  cl_assert_equal_i(s_num_timeline_adds, 17);
  cl_assert_equal_i(s_num_timeline_removes, 11);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());
}

void test_alarm__just_once_alarm_timeout_ahead(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  // It's currently Thursday @ 00:00
  bool just_once_schedule_thursday[7] = {false, false, false, false, true, false, false};
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  // Only 1 pin should be added
  cl_assert_equal_i(s_num_timeline_adds, 1);
}

void test_alarm__just_once_alarm_timeout_behind(void) {
  AlarmId id;
  s_current_hour = 11;
  s_current_minute = 30;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  bool just_once_schedule_friday[7] = {false, false, false, false, false, true, false};
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_JUST_ONCE, just_once_schedule_friday);
  // Only 1 pin should be added
  cl_assert_equal_i(s_num_timeline_adds, 1);
}

void test_alarm__just_once_alarm(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  // It's currently Thursday @ 00:00
  bool just_once_schedule_thursday[7] = {false, false, false, false, true, false, false};
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  // Only 1 pin should be added
  cl_assert_equal_i(s_num_timeline_adds, 1);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  // Only 1 pin should be added
  cl_assert_equal_i(s_num_timeline_adds, 2);

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  prv_assert_alarm_config(id1, 10, 30, true, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 4);
  cl_assert_equal_i(s_num_timeline_removes, 2);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. No alarms should be up
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  prv_assert_alarm_config(id2, 11, 30, true, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(s_num_timeline_adds, 5);
  cl_assert_equal_i(s_num_timeline_removes, 3);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());
}

void test_alarm__custom_alarm_everyday(void) {
  AlarmId id1;

  bool schedule_1[7] = {true, true, true, true, true, true, true};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_CUSTOM, schedule_1);

  // It's currently Thursday @ 00:00.

  s_current_day = s_friday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);

  s_current_day = s_sunday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);

  s_current_day = s_monday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 4);

  s_current_day = s_tuesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 5);

  s_current_day = s_wednesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 6);
}

void test_alarm__custom_alarm_weekends_and_weekday(void) {
  AlarmId id1;

  bool schedule_1[7] = {true, false, false, true, false, false, true};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_CUSTOM, schedule_1);

  // It's currently Thursday @ 00:00.

  s_current_day = s_friday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  s_current_day = s_sunday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_monday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);

  s_current_day = s_tuesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);

  s_current_day = s_wednesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
}

void test_alarm__custom_alarm_partial_weekdays(void) {
  AlarmId id1;

  bool schedule_1[7] = {false, true, true, true, true, false, false};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_CUSTOM, schedule_1);

  // It's currently Thursday @ 00:00.

  s_current_day = s_friday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_sunday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_monday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_tuesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);

  s_current_day = s_wednesday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);
}

void test_alarm__custom_alarm_weekends(void) {
  AlarmId id1, id2;

  bool schedule_1[7] = {true, false, false, false, false, false, true};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_CUSTOM, schedule_1);

  // Only 1 pin should be added (for Saturday)
  cl_assert_equal_i(s_num_timeline_adds, 1);

  id2 = alarm_create(&(AlarmInfo){
    .hour = 11,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_CUSTOM, schedule_1);
  cl_assert_equal_i(s_num_timeline_adds, 2);

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_saturday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 2);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(s_num_timeline_adds, 9);
  cl_assert_equal_i(s_num_timeline_removes, 5);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // First alarm goes off. Second one should be up
  s_current_hour = 10;
  s_current_minute = 30;
  s_current_day = s_sunday;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 3);
  cl_assert_equal_i(s_num_alarm_events_put, 3);
  cl_assert_equal_i(s_num_timeline_adds, 11);
  cl_assert_equal_i(s_num_timeline_removes, 7);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());

  // Second alarm goes off. First one should be up again, but not until Saturday
  s_current_hour = 11;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 4);
  cl_assert_equal_i(s_num_alarm_events_put, 4);
  cl_assert_equal_i(s_num_timeline_adds, 12);
  cl_assert_equal_i(s_num_timeline_removes, 8);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, pbl_realtime());
}

void test_alarm__custom_alarm_no_alarm(void) {
  AlarmId id1;

  bool schedule_1[7] = {false, false, false, false, false, false, false};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_CUSTOM, schedule_1);

  cl_assert_equal_i(s_alarm_timer_timeout_ms, 0);
}

void test_alarm__custom_alarm_multiple(void) {
  AlarmId id1, id2;

  // Alarm set for Tuesday and Saturday
  bool schedule_1[7] = {false, false, true, false, false, false, true};
  id1 = alarm_create(&(AlarmInfo){
    .hour = 1,
    .minute = 30,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_1
  });
  prv_assert_alarm_config(id1, 1, 30, false, ALARM_KIND_CUSTOM, schedule_1);
  // It's currently Thursday @ 00:00, 2 days + 1:30 till next alarm
  /* cl_assert_equal_i(s_alarm_timer_timeout_ms, */
  /*                   (SECONDS_PER_DAY * 2 + prv_hours_and_minutes_to_seconds(1, 30)) * 1000); */

  s_current_day = s_sunday;
  s_current_hour = 12;
  s_current_minute = 15;
  pbl_cron_wakeup();
  bool schedule_2[7] = {false, true, false, false, false, false, false};
  id2 = alarm_create(&(AlarmInfo){
    .hour = 13,
    .minute = 15,
    .kind = ALARM_KIND_CUSTOM,
    .scheduled_days = &schedule_2
  });
  prv_assert_alarm_config(id2, 13, 15, false, ALARM_KIND_CUSTOM, schedule_2);
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_day = s_tuesday;
  s_current_hour = 1;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
}

void test_alarm__disable_upcoming_alarm(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  // Disable the 10:30 alarm
  alarm_set_enabled(id1, false);

  // The 10:30 alarm should not have gone off
  s_current_hour = 11;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  // Disable the 11:30 alarm
  alarm_set_enabled(id2, false);

  // The 11:30 alarm should not go off either
  s_current_hour = 12;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  // Enable the 11:30 alarm - now it should go off
  s_current_hour = 11;
  alarm_set_enabled(id2, true);
  s_current_hour = 12;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__delete_upcoming_alarm(void) {
  AlarmId id1, id2;
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id1, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  id2 = alarm_create(&(AlarmInfo){.hour = 11, .minute = 30, .kind = ALARM_KIND_EVERYDAY});
  prv_assert_alarm_config(id2, 11, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  // Delete the 10:30 alarm
  alarm_delete(id1);

  // The 10:30 alarm should not go off
  s_current_hour = 11;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  // Delete the 11:30 alarm
  alarm_delete(id2);

  // The 11:30 alarm should not go off either
  s_current_hour = 12;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
}

void test_alarm__alarm_type_change_updates_timeout(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_WEEKDAYS, s_weekday_schedule);
  // 2 pins should be added (for Thursday / Friday)
  cl_assert_equal_i(s_num_timeline_adds, 2);

  alarm_set_kind(id, ALARM_KIND_WEEKENDS);

  // Alarm should not go off on Thursday anymore
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);

  // Alarm should go off on the weekend
  s_current_day = s_saturday;
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__alarm_get_next_enabled_alarm(void) {
  // Declarations
  time_t next_alarm = 0;
  time_t time_until_alarm = 0;
  bool at_least_one_alarm_enabled = false;
  bool alarm_is_scheduled = false;
  AlarmId id1, id2;

  // No alarms scheduled
  at_least_one_alarm_enabled = alarm_get_next_enabled_alarm(&next_alarm);
  cl_assert_equal_b(at_least_one_alarm_enabled, false);
  cl_assert_equal_i(next_alarm, 0);

  // Schedule an alarm, it becomes the next alarm
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  at_least_one_alarm_enabled = alarm_get_next_enabled_alarm(&next_alarm);
  cl_assert_equal_b(at_least_one_alarm_enabled, true);
  alarm_is_scheduled = alarm_get_time_until(id1, &time_until_alarm);
  cl_assert_equal_b(alarm_is_scheduled, true);
  cl_assert_equal_i(next_alarm, pbl_realtime() + time_until_alarm);

  // Schedule another alarm before the previous alarm, it becomes the next alarm
  id2 = alarm_create(&(AlarmInfo){.hour = 9, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  at_least_one_alarm_enabled = alarm_get_next_enabled_alarm(&next_alarm);
  cl_assert_equal_b(at_least_one_alarm_enabled, true);
  alarm_is_scheduled = alarm_get_time_until(id2, &time_until_alarm);
  cl_assert_equal_b(alarm_is_scheduled, true);
  cl_assert_equal_i(next_alarm, pbl_realtime() + time_until_alarm);

  // Disable both alarms, now there is no next alarm
  alarm_set_enabled(id1, false);
  alarm_set_enabled(id2, false);
  at_least_one_alarm_enabled = alarm_get_next_enabled_alarm(&next_alarm);
  cl_assert_equal_b(at_least_one_alarm_enabled, false);
}

void test_alarm__alarm_is_next_enabled_alarm_smart(void) {
  // Declarations
  AlarmId id1;
  AlarmId id2;
  AlarmId id3;

  // No alarms scheduled, our function should return false
  cl_assert_equal_b(alarm_is_next_enabled_alarm_smart(), false);

  // Schedule a basic (non-smart) alarm, our function should return false
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  cl_assert_equal_b(alarm_is_next_enabled_alarm_smart(), false);

  // Schedule a smart alarm before the basic alarm, our function should return true
  id2 = alarm_create(&(AlarmInfo){.hour = 9, .minute = 30, .is_smart = true});
  cl_assert_equal_b(alarm_is_next_enabled_alarm_smart(), true);

  // Schedule another basic alarm before the smart alarm, our function should return false again
  id3 = alarm_create(&(AlarmInfo){.hour = 8, .minute = 30, .kind = ALARM_KIND_WEEKDAYS});
  cl_assert_equal_b(alarm_is_next_enabled_alarm_smart(), false);

  // Disable all three alarms, now there is no next alarm and so our function should return false
  alarm_set_enabled(id1, false);
  alarm_set_enabled(id2, false);
  alarm_set_enabled(id3, false);
  cl_assert_equal_b(alarm_is_next_enabled_alarm_smart(), false);
}

void test_alarm__skip_two_alarms(void) {
  AlarmId id1, id2;
  bool just_once_schedule_thursday[7] = {false, false, false, false, true, false, false};
  id1 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 10, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id1, 10, 10, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);

  id2 = alarm_create(&(AlarmInfo){.hour = 10, .minute = 20, .kind = ALARM_KIND_JUST_ONCE});
  prv_assert_alarm_config(id2, 10, 20, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);

  // Skip ahead of both alarms
  // One of the alarms should go off
  s_current_hour = 10;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // The other alarm should not go off
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Alarms missed while the watch was down

//! Restarts the alarm service the way a reboot would, leaving the settings file intact.
static void prv_simulate_reboot(void) {
  pbl_cron_deinit();
  pbl_cron_init();
  alarm_init();
  alarm_service_enable_alarms(true);
}

//! Restarts the alarm service but leaves alarms disabled so a pending catch-up can be inspected.
static void prv_simulate_reboot_without_enabling(void) {
  pbl_cron_deinit();
  pbl_cron_init();
  alarm_init();
}

void test_alarm__missed_alarm_cleared_when_slot_deleted(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot_without_enabling();

  alarm_delete(id);
  alarm_service_enable_alarms(true);
  cl_assert_equal_i(s_num_alarms_fired, 0);
}

void test_alarm__missed_alarm_cleared_when_updated(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot_without_enabling();

  const AlarmInfo update = {
    .hour = 7,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);

  alarm_service_enable_alarms(true);
  cl_assert_equal_i(s_num_alarms_fired, 0);
}

void test_alarm__missed_alarm_cleared_when_slot_reused(void) {
  struct pbl_settings_file file;
  cl_must_pass(pbl_settings_open(&file, "/alarms", 1024));
  const AlarmArmedRecord armed = {
    .time = s_current_day + 6 * PBL_SEC_PER_HOUR,
    .id = 0,
  };
  cl_must_pass(pbl_settings_set(&file, "ArmedAlarm", strlen("ArmedAlarm"), &armed, sizeof(armed)));
  pbl_settings_close(&file);

  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot_without_enabling();

  AlarmId id = alarm_create(&(AlarmInfo){.hour = 7, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  cl_assert_equal_i(id, 0);

  alarm_service_enable_alarms(true);
  cl_assert_equal_i(s_num_alarms_fired, 0);
}

//! Queue a missed-alarm catch-up and leave its work callback pending.
static void prv_queue_missed_alarm_catchup(void) {
  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot_without_enabling();

  s_defer_sys_work_callbacks = true;
  alarm_service_enable_alarms(true);
  s_defer_sys_work_callbacks = false;
  cl_assert(s_pending_sys_work_callback);
}

void test_alarm__queued_missed_alarm_rejected_after_update(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  prv_queue_missed_alarm_catchup();

  const AlarmInfo update = {
    .hour = 7,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);
}

void test_alarm__queued_missed_alarm_rejected_after_delete_and_recreate(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  prv_queue_missed_alarm_catchup();

  alarm_delete(id);
  AlarmId new_id = alarm_create(&(AlarmInfo){.hour = 7, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  cl_assert_equal_i(new_id, id);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm__queued_missed_alarm_rejected_after_disable(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  prv_queue_missed_alarm_catchup();

  alarm_set_enabled(id, false);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm__queued_missed_alarm_survives_tone_update(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  prv_queue_missed_alarm_catchup();

  const AlarmInfo update = {
    .hour = 6,
    .minute = 0,
    .kind = ALARM_KIND_EVERYDAY,
    .tone = AlarmTone_Bell,
  };
  cl_assert_equal_i(alarm_update_checked(id, &update), AlarmUpdateResultOk);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(alarm_get_most_recent_id(), id);
}

void test_alarm__queued_timer_callback_rejected_after_delete(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 0;
  s_defer_sys_work_callbacks = true;
  pbl_cron_wakeup();
  s_defer_sys_work_callbacks = false;
  cl_assert(s_pending_sys_work_callback);

  alarm_delete(id);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm__smart_alarm_tracking_across_boot(void) {
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 10,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // At boot, activity has not started yet, so the alarm is scheduled for 10:30
  // without the early window.
  s_activity_tracking_on = false;
  prv_simulate_reboot();
  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  // Tracking starting must reconcile the schedule to the 10:00 early window.
  cl_assert(s_activity_event_info);
  s_activity_tracking_on = true;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  // Tracking stopping must remove the early window again.
  s_activity_tracking_on = false;
  PebbleEvent stopped = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStopped},
  };
  s_activity_event_info->handler(&stopped, s_activity_event_info->context);
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);
}

void test_alarm__tracking_start_inside_smart_window_keeps_alarm(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // Boot at 07:10 with tracking off: the alarm is pending for 07:30.
  s_current_hour = 7;
  s_current_minute = 10;
  s_activity_tracking_on = false;
  prv_simulate_reboot();

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 20 * PBL_SEC_PER_MIN);

  // Tracking starts inside the window. The 07:00 early time is already past, so
  // keep the pending 07:30 occurrence instead of advancing to tomorrow.
  s_activity_tracking_on = true;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);

  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 20 * PBL_SEC_PER_MIN);
  cl_assert(!alarm_is_next_enabled_alarm_smart());

  time_t time_until = 0;
  cl_assert(alarm_get_time_until(id, &time_until));
  cl_assert_equal_i(time_until, 20 * PBL_SEC_PER_MIN);

  // A retained sleep reading must not start a smart snooze after the fallback.
  s_sleep_state = ActivitySleepStateLightSleep;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert(!fake_work_is_scheduled(&s_snooze_timer));
}

void test_alarm__tracking_start_at_smart_window_start_keeps_alarm(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // Boot just before the window opens, with tracking off.
  s_current_hour = 6;
  s_current_minute = 59;
  s_activity_tracking_on = false;
  prv_simulate_reboot();

  s_current_hour = 7;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm - pbl_realtime(), 30 * PBL_SEC_PER_MIN);

  time_t time_until = 0;
  cl_assert(alarm_get_time_until(id, &time_until));
  cl_assert_equal_i(time_until, 30 * PBL_SEC_PER_MIN);
  cl_assert(!alarm_is_next_enabled_alarm_smart());
}

void test_alarm__tracking_start_at_alarm_time_fires_once(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // Boot exactly at the configured time, before the due cron callback runs.
  s_current_hour = 7;
  s_current_minute = 30;
  s_activity_tracking_on = false;
  prv_simulate_reboot();

  s_activity_tracking_on = true;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm, pbl_realtime());

  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // A second tracking event after the callback must not schedule or fire it again.
  s_activity_event_info->handler(&started, s_activity_event_info->context);
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert(next_alarm > pbl_realtime());
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__early_smart_alarm_does_not_fire_again_at_time(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  s_current_hour = 7;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__early_smart_alarm_tracking_off_reload_does_not_refire(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // Tracking turns off before the early callback runs. The reload must not
  // schedule the already-fired 07:30 occurrence with an offset-0 cron.
  s_current_hour = 7;
  s_current_minute = 0;
  s_activity_tracking_on = false;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert(next_alarm > pbl_realtime());

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm__next_days_occurrence_can_fire_at_exact_time(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  s_current_hour = 7;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // The same alarm ID is still most recent from Thursday. Friday's occurrence
  // must not be mistaken for Thursday's already-fired one.
  s_current_day = s_friday;
  s_current_hour = 7;
  s_current_minute = 30;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm, pbl_realtime());

  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
}

void test_alarm__reused_just_once_occurrence_can_fire_at_exact_time(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 7, .minute = 30, .kind = ALARM_KIND_JUST_ONCE});
  cl_assert(id != ALARM_INVALID_ID);

  s_current_hour = 7;
  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // Re-enable the same just-once ID for the next day.
  alarm_set_enabled(id, true);
  s_current_day = s_friday;
  s_current_hour = 7;
  s_current_minute = 30;
  PebbleEvent started = {
    .type = PEBBLE_ACTIVITY_EVENT,
    .activity_event = {.type = PebbleActivityEvent_TrackingStarted},
  };
  s_activity_event_info->handler(&started, s_activity_event_info->context);

  time_t next_alarm = 0;
  cl_assert(alarm_get_next_enabled_alarm(&next_alarm));
  cl_assert_equal_i(next_alarm, pbl_realtime());

  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
}

void test_alarm__missed_smart_alarm_does_not_fire_again_at_time(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  // The watch was down when the 07:00 early window fired and boots at 07:02.
  // The catch-up fires once; the configured 07:30 occurrence must not fire again.
  s_current_hour = 7;
  s_current_minute = 2;
  prv_simulate_reboot();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm__missed_alarm_fires_after_reboot(void) {
  // It's currently Thursday @ 00:00, so the alarm is armed for Thursday @ 06:00
  alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  // The watch was down over 06:00 and comes back up shortly after
  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm__missed_alarm_not_fired_when_too_late(void) {
  alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 10;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 0);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm__alarm_not_fired_when_reboot_precedes_it(void) {
  alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 5;
  s_current_minute = 59;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 0);

  // The alarm is still armed and goes off at its own time
  s_current_hour = 6;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__missed_disabled_alarm_not_fired_after_reboot(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  alarm_set_enabled(id, false);

  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 0);
}

void test_alarm__missed_alarm_not_fired_twice_after_second_reboot(void) {
  alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 2;
  prv_simulate_reboot();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  // The alarm is now armed for tomorrow, so booting again must not fire it
  s_current_minute = 4;
  prv_simulate_reboot();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__missed_just_once_alarm_rearmed_for_next_day(void) {
  // It's currently Thursday @ 00:00
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_JUST_ONCE});
  bool just_once_schedule_thursday[7] = {false, false, false, false, true, false, false};
  prv_assert_alarm_config(id, 6, 0, false, ALARM_KIND_JUST_ONCE, just_once_schedule_thursday);

  // The watch was down over 06:00 and comes back up too late to catch the alarm up. The alarm
  // must not stay armed for Thursday next week.
  s_current_hour = 8;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 0);
  bool just_once_schedule_friday[7] = {false, false, false, false, false, true, false};
  prv_assert_alarm_config(id, 6, 0, false, ALARM_KIND_JUST_ONCE, just_once_schedule_friday);

  s_current_day = s_friday;
  s_current_hour = 6;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
}

void test_alarm__missed_just_once_alarm_fires_and_is_disabled(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_JUST_ONCE});

  s_current_hour = 6;
  s_current_minute = 3;
  prv_simulate_reboot();

  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_b(alarm_get_enabled(id), false);
}

void test_alarm__smart_alarm_does_not_refire_after_another_alarm(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId smart = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  AlarmId basic = alarm_create(&(AlarmInfo){.hour = 7, .minute = 10, .kind = ALARM_KIND_EVERYDAY});
  cl_assert(smart != ALARM_INVALID_ID && basic != ALARM_INVALID_ID);

  s_current_hour = 7;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);

  s_current_minute = 10;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 2);
}

void test_alarm__reenable_after_smart_fire_does_not_refire(void) {
  s_current_hour = 6;
  s_current_minute = 0;
  s_activity_tracking_on = true;
  AlarmId id = alarm_create(&(AlarmInfo){
    .hour = 7,
    .minute = 30,
    .kind = ALARM_KIND_EVERYDAY,
    .is_smart = true,
  });
  cl_assert(id != ALARM_INVALID_ID);

  s_current_hour = 7;
  s_current_minute = 0;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  // Toggling the alarm off and on must not re-arm the occurrence that already fired.
  s_current_minute = 5;
  alarm_set_enabled(id, false);
  s_current_minute = 6;
  alarm_set_enabled(id, true);

  s_current_minute = 30;
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm__queued_timer_callback_rejected_after_delete_and_recreate(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 6, .minute = 0, .kind = ALARM_KIND_EVERYDAY});

  s_current_hour = 6;
  s_current_minute = 0;
  s_defer_sys_work_callbacks = true;
  pbl_cron_wakeup();
  s_defer_sys_work_callbacks = false;
  cl_assert(s_pending_sys_work_callback);

  alarm_delete(id);
  AlarmId new_id = alarm_create(&(AlarmInfo){.hour = 21, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  cl_assert_equal_i(new_id, id);

  s_pending_sys_work_callback(s_pending_sys_work_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

// TODO:
// - Test disable while snoozing
// - Test delete while snoozing
