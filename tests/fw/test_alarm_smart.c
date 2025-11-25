/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_alarm_common.h"

// Fakes
#include "fake_rtc.h"
#include "fake_new_timer.h"

#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"

static int s_rand = 0;

int rand(void) {
  // There are no odds
  return s_rand;
}

static ActivitySleepState s_sleep_state = ActivitySleepStateAwake;
static uint16_t s_sleep_state_seconds = 0;
static uint16_t s_last_vmc = 0;

bool activity_tracking_on(void) {
  return true;
}

bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  cl_assert_equal_i(history_len, 1);
  if (metric == ActivityMetricSleepState) {
    *history = s_sleep_state;
    return true;
  } else if (metric == ActivityMetricSleepStateSeconds) {
    *history = s_sleep_state_seconds;
    return true;
  } else if (metric == ActivityMetricLastVMC) {
    *history = s_last_vmc;
    return true;
  }

  cl_assert(false);
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Helper Functions

static void prv_set_time(time_t day, int hour, int minute) {
  s_current_day = day;
  s_current_hour = hour;
  s_current_minute = minute;
  rtc_set_time(day + prv_hours_and_minutes_to_seconds(hour, minute));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Setup

void test_alarm_smart__initialize(void) {
  s_num_timeline_adds = 0;
  s_num_timeline_removes = 0;
  s_num_alarm_events_put = 0;
  s_num_alarms_fired = 0;
  s_last_vmc = 0;
  s_rand = 0;

  // Setup time
  TimezoneInfo tz_info = {
    .tm_zone = "UTC",
  };
  time_util_update_timezone(&tz_info);
  rtc_set_timezone(&tz_info);

  // Default to Thursday
  prv_set_time(s_thursday, 0, 0);

  timeline_item_destroy(s_last_timeline_item_added);
  s_last_timeline_item_added = NULL;
  s_last_timeline_item_removed_uuid = (Uuid) {};

  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);

  cron_service_init();

  alarm_init();
  alarm_service_enable_alarms(true);
}

void test_alarm_smart__cleanup(void) {
  cron_service_deinit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Smart alarms

#define SMART_ALARM_UPDATE_MIN (SMART_ALARM_SNOOZE_DELAY_S / SECONDS_PER_MINUTE)

void test_alarm_smart__trigger_30_min_early_awake(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo) { .hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Set sleep status
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;

  time_t next_alarm_time;
  alarm_get_next_enabled_alarm(&next_alarm_time);
  cl_assert_equal_i(next_alarm_time,
                    s_current_day + 10 * SECONDS_PER_HOUR + 30 * SECONDS_PER_MINUTE);

  // Don't trigger too early
  prv_set_time(s_current_day, 9, 49);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Trigger at the right time
  prv_set_time(s_current_day, 10, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 3);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__trigger_30_min_early_vmc(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo) { .hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  s_sleep_state = ActivitySleepStateLightSleep;
  s_last_vmc = 1;
  prv_set_time(s_current_day, 10, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__dont_trigger_30_min_early_deep_sleep(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo) { .hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;
  prv_set_time(s_current_day, 10, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm_smart__trigger_15_min_early_light_sleep(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo) { .hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Begin light sleep
  s_sleep_state = ActivitySleepStateLightSleep;
  s_sleep_state_seconds = SMART_ALARM_MAX_LIGHT_SLEEP_S - 15 * SECONDS_PER_MINUTE;

  // Smart alarms are first triggered by cron at T-30min
  prv_set_time(s_current_day, 10, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Afterwards, the alarm snooze timer triggers every 5min
  const int num_checks = 3;
  for (int i = 0; i < num_checks; i++) {
    // Step forward time and increase light sleep duration
    s_sleep_state_seconds += 5 * SECONDS_PER_MINUTE;
    s_last_vmc = i == 2 ? 1 : 0;
    prv_set_time(s_current_day, 10, (i + 1) * 5);
    PBL_LOG(LOG_LEVEL_DEBUG, "Iteration #%d, sleep %d seconds", i, s_sleep_state_seconds);
    stub_new_timer_invoke(1);
    if (i < num_checks - 1) {
      // Smart alarm non-trigger checks
      cl_assert_equal_i(s_num_alarms_fired, 1);
      cl_assert_equal_i(s_num_alarm_events_put, 0);
    }
  }

  // Smart alarm trigger checks
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 3);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__trigger_at_timeout(void) {
  AlarmId id;
  id = alarm_create(&(AlarmInfo) { .hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Stay in deep sleep
  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;

  // Make sure random snooze does not cause the smart alarm to go beyond the alarm time
  s_rand = 4;

  // Smart alarms are first triggered by cron at T-30min
  prv_set_time(s_current_day, 10, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Afterwards, the alarm snooze timer triggers every 5min
  const int num_checks = 6;
  for (int i = 0; i < num_checks; i++) {
    // Step forward time and increase light sleep duration
    s_sleep_state_seconds = (i + 1) * 5 * SECONDS_PER_MINUTE;
    s_last_vmc = (i == 5);
    prv_set_time(s_current_day, 10, i * 5);
    PBL_LOG(LOG_LEVEL_DEBUG, "Iteration #%d, sleep %d seconds", i, s_sleep_state_seconds);
    stub_new_timer_invoke(1);
    if (i < num_checks - 1) {
      // Smart alarm non-trigger checks
      cl_assert_equal_i(s_num_alarms_fired, 1);
      cl_assert_equal_i(s_num_alarm_events_put, 0);
    }
  }

  // Smart alarm trigger checks
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 3);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__across_midnight_boundary(void) {
  prv_set_time(s_sunday, 22, 0);

  AlarmId id;
  bool monday_only[7] = {false, true, false, false, false, false, false};
  id = alarm_create(&(AlarmInfo) { .hour = 0, .minute = 15, .kind = ALARM_KIND_CUSTOM, .is_smart = true,
                                   .scheduled_days = &monday_only });
  prv_assert_alarm_config(id, 0, 15, false, ALARM_KIND_CUSTOM, monday_only);
  cl_assert_equal_i(s_num_timeline_adds, 1);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Set sleep status
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;

  // Don't trigger too early
  prv_set_time(s_sunday, 23, 44);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Trigger at the right time
  prv_set_time(s_sunday, 23, 45);
  cron_service_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 2);
  cl_assert_equal_i(s_num_timeline_removes, 1);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}
