/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_alarm_common.h"

// Fakes
#include <pbl/services/time.h>
#include <pbl/util/units.h>

#include <fake_new_timer.h>
#include <fake_rtc.h>
#include <stubs_blob_db_sync.h>
#include <stubs_blob_db_sync_util.h>

static int s_rand = 0;
static TimerID s_alarm_snooze_timer_id;

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
  s_defer_system_task_callbacks = false;
  s_pending_system_task_callback = NULL;
  s_pending_system_task_data = NULL;
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
  s_last_timeline_item_removed_uuid = (Uuid){};
  memset(s_fake_pin_records, 0, sizeof(s_fake_pin_records));

  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);

  pbl_cron_init();

  alarm_init();
  s_alarm_snooze_timer_id = ((StubTimer *)s_idle_timers)->id;
  alarm_service_enable_alarms(true);
}

void test_alarm_smart__cleanup(void) {
  alarm_dismiss_alarm();
  if (alarm_get_most_recent_id() != ALARM_INVALID_ID) {
    alarm_delete(alarm_get_most_recent_id());
  }
  pbl_cron_deinit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Smart alarms

#define SMART_ALARM_UPDATE_MIN (SMART_ALARM_SNOOZE_DELAY_S / PBL_SEC_PER_MIN)

void test_alarm_smart__trigger_30_min_early_awake(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Set sleep status
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;

  time_t next_alarm_time;
  alarm_get_next_enabled_alarm(&next_alarm_time);
  cl_assert_equal_i(next_alarm_time, s_current_day + 10 * PBL_SEC_PER_HOUR + 30 * PBL_SEC_PER_MIN);

  // Don't trigger too early
  prv_set_time(s_current_day, 9, 49);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Trigger at the right time
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 6);
  cl_assert_equal_i(s_num_timeline_removes, 3);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__trigger_30_min_early_vmc(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  s_sleep_state = ActivitySleepStateLightSleep;
  s_last_vmc = 1;
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

void test_alarm_smart__dont_trigger_30_min_early_deep_sleep(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm_smart__trigger_15_min_early_light_sleep(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);
  cl_assert_equal_i(s_num_timeline_adds, 3);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Begin light sleep
  s_sleep_state = ActivitySleepStateLightSleep;
  s_sleep_state_seconds = SMART_ALARM_MAX_LIGHT_SLEEP_S - 15 * PBL_SEC_PER_MIN;

  // Smart alarms are first triggered by cron at T-30min
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Afterwards, the alarm snooze timer triggers every 5min
  const int num_checks = 3;
  for (int i = 0; i < num_checks; i++) {
    // Step forward time and increase light sleep duration
    s_sleep_state_seconds += 5 * PBL_SEC_PER_MIN;
    s_last_vmc = i == 2 ? 1 : 0;
    prv_set_time(s_current_day, 10, (i + 1) * 5);
    PBL_LOG_DBG("Iteration #%d, sleep %d seconds", i, s_sleep_state_seconds);
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
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
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
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Afterwards, the alarm snooze timer triggers every 5min
  const int num_checks = 6;
  for (int i = 0; i < num_checks; i++) {
    // Step forward time and increase light sleep duration
    s_sleep_state_seconds = (i + 1) * 5 * PBL_SEC_PER_MIN;
    s_last_vmc = (i == 5);
    prv_set_time(s_current_day, 10, i * 5);
    PBL_LOG_DBG("Iteration #%d, sleep %d seconds", i, s_sleep_state_seconds);
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

void test_alarm_smart__user_snooze_fires_after_delay(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  // Awake, so the smart alarm fires immediately at T-30min
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  // The user snoozes the firing alarm, then looks asleep again
  alarm_set_snooze_alarm();
  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_last_vmc = 0;

  // The snooze timer must re-fire the alarm event, not re-enter the sleep poll
  prv_set_time(s_current_day, 10, alarm_get_snooze_delay());
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 2);

  // Snoozing again keeps working the same way
  alarm_set_snooze_alarm();
  prv_set_time(s_current_day, 10, 2 * alarm_get_snooze_delay());
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 3);
}

void test_alarm_smart__user_snooze_survives_clock_change(void) {
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  // Stay asleep so the sleep poll runs and drives up the smart snooze counter
  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;
  s_rand = 4;

  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  const int num_checks = 6;
  for (int i = 0; i < num_checks; i++) {
    s_sleep_state_seconds = (i + 1) * 5 * PBL_SEC_PER_MIN;
    s_last_vmc = (i == 5);
    prv_set_time(s_current_day, 10, i * 5);
    stub_new_timer_invoke(1);
  }
  // The alarm has now fired at the end of the smart window
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  // The user snoozes it, then a clock change arrives (phone time sync, DST, RTC correction)
  alarm_set_snooze_alarm();
  alarm_handle_clock_change();

  // The snooze must survive: no immediate re-fire
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  // ...and it still fires after exactly the configured delay
  prv_set_time(s_current_day, 10, 25 + alarm_get_snooze_delay());
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
}

void test_alarm_smart__clock_change_still_force_triggers_sleep_poll(void) {
  // Guards the FIRM-3127 fix: with no user snooze pending, a clock change during the smart
  // window must still force the alarm to fire rather than silently dropping it.
  AlarmId id;
  id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  prv_assert_alarm_config(id, 10, 30, false, ALARM_KIND_EVERYDAY, s_every_day_schedule);

  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;
  s_rand = 4;

  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // A couple of sleep polls, so the smart snooze counter is non-zero but the alarm has not fired
  for (int i = 0; i < 2; i++) {
    s_sleep_state_seconds = (i + 1) * 5 * PBL_SEC_PER_MIN;
    s_last_vmc = 0;
    prv_set_time(s_current_day, 10, i * 5);
    stub_new_timer_invoke(1);
  }
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Clock change lands inside the smart window with no user snooze pending
  prv_set_time(s_current_day, 10, 35);
  alarm_handle_clock_change();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm_smart__across_midnight_boundary(void) {
  prv_set_time(s_sunday, 22, 0);

  AlarmId id;
  bool monday_only[7] = {false, true, false, false, false, false, false};
  id = alarm_create(&(AlarmInfo){
    .hour = 0,
    .minute = 15,
    .kind = ALARM_KIND_CUSTOM,
    .is_smart = true,
    .scheduled_days = &monday_only
  });
  prv_assert_alarm_config(id, 0, 15, false, ALARM_KIND_CUSTOM, monday_only);
  cl_assert_equal_i(s_num_timeline_adds, 1);
  cl_assert_equal_i(s_num_timeline_removes, 0);

  // Set sleep status
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;

  // Don't trigger too early
  prv_set_time(s_sunday, 23, 44);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 0);
  cl_assert_equal_i(s_num_alarm_events_put, 0);

  // Trigger at the right time
  prv_set_time(s_sunday, 23, 45);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarms_fired, 1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(s_num_timeline_adds, 2);
  cl_assert_equal_i(s_num_timeline_removes, 1);
  cl_assert_equal_i(s_last_timeline_item_added->header.timestamp, rtc_get_time());
}

static AlarmId prv_start_sleeping_smart_alarm(void) {
  AlarmId id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  s_sleep_state = ActivitySleepStateRestfulSleep;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(alarm_get_most_recent_id(), id);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
  cl_assert(stub_new_timer_is_scheduled(s_alarm_snooze_timer_id));
  return id;
}

static void prv_assert_sleep_poll_cancelled(TimerID timer_id) {
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);
  alarm_handle_clock_change();
  cl_assert_equal_i(s_num_alarm_events_put, 0);
}

void test_alarm_smart__editing_time_and_type_cancels_sleep_poll(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_time(id, 14, 30);
  alarm_set_smart(id, false);
  prv_assert_sleep_poll_cancelled(timer_id);

  s_sleep_state = ActivitySleepStateAwake;
  prv_set_time(s_current_day, 10, 30);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 0);
  prv_set_time(s_current_day, 14, 30);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(alarm_get_most_recent_id(), id);
}

void test_alarm_smart__editing_type_cancels_sleep_poll(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_smart(id, false);
  prv_assert_sleep_poll_cancelled(timer_id);

  prv_set_time(s_current_day, 10, 30);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm_smart__editing_kind_cancels_sleep_poll(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_kind(id, ALARM_KIND_WEEKENDS);
  prv_assert_sleep_poll_cancelled(timer_id);
}

void test_alarm_smart__editing_custom_days_cancels_sleep_poll(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_custom(id, s_weekend_schedule);
  prv_assert_sleep_poll_cancelled(timer_id);
}

void test_alarm_smart__editing_other_alarm_preserves_sleep_poll(void) {
  AlarmId other_id =
      alarm_create(&(AlarmInfo){.hour = 15, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_time(other_id, 16, 0);
  alarm_set_smart(other_id, true);
  alarm_set_kind(other_id, ALARM_KIND_WEEKENDS);
  alarm_set_custom(other_id, s_weekday_schedule);
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(alarm_get_most_recent_id(), id);

  s_sleep_state = ActivitySleepStateAwake;
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  // Recording the first firing must retain the alarm for later manual snoozes.
  cl_assert_equal_i(alarm_get_most_recent_id(), id);
  alarm_set_snooze_alarm();
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
}

void test_alarm_smart__editing_alert_settings_preserves_user_snooze(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  s_sleep_state = ActivitySleepStateAwake;
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  alarm_set_sound_enabled(id, true);
  alarm_set_vibrate_enabled(id, false);
  alarm_set_tone(id, AlarmTone_Bell);
  cl_assert_equal_i(alarm_get_most_recent_id(), id);

  alarm_set_snooze_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_tone(id, AlarmTone_Chime);
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  s_sleep_state = ActivitySleepStateRestfulSleep;
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
}

void test_alarm_smart__editing_basic_alarm_cancels_user_snooze(void) {
  AlarmId id = alarm_create(&(AlarmInfo){.hour = 10, .minute = 0, .kind = ALARM_KIND_EVERYDAY});
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  alarm_set_snooze_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_time(id, 14, 0);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);
  alarm_handle_clock_change();
  cl_assert_equal_i(s_num_alarm_events_put, 1);

  prv_set_time(s_current_day, 14, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 2);
}

void test_alarm_smart__editing_alarm_rejects_queued_snooze_callback(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  s_defer_system_task_callbacks = true;
  stub_new_timer_invoke(1);
  s_defer_system_task_callbacks = false;
  cl_assert(s_pending_system_task_callback);

  alarm_set_time(id, 14, 30);
  s_sleep_state = ActivitySleepStateAwake;
  s_pending_system_task_callback(s_pending_system_task_data);
  cl_assert_equal_i(s_num_alarm_events_put, 0);
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_alarm_smart__queued_snooze_cannot_process_new_alarm(void) {
  AlarmId id = prv_start_sleeping_smart_alarm();
  s_defer_system_task_callbacks = true;
  stub_new_timer_invoke(1);
  s_defer_system_task_callbacks = false;
  cl_assert(s_pending_system_task_callback);
  alarm_set_time(id, 14, 30);

  AlarmId other_id =
      alarm_create(&(AlarmInfo){.hour = 10, .minute = 1, .kind = ALARM_KIND_EVERYDAY});
  prv_set_time(s_current_day, 10, 1);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  cl_assert_equal_i(alarm_get_most_recent_id(), other_id);
  alarm_set_snooze_alarm();
  s_pending_system_task_callback(s_pending_system_task_data);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 2);
}

void test_alarm_smart__anonymous_user_snooze_still_fires(void) {
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);
  alarm_set_snooze_alarm();
  prv_set_time(s_current_day, 0, alarm_get_snooze_delay());
  stub_new_timer_invoke(1);
  cl_assert_equal_i(s_num_alarm_events_put, 1);
}

void test_alarm_smart__switching_to_basic_cancels_user_snooze(void) {
  AlarmId id = alarm_create(
      &(AlarmInfo){.hour = 10, .minute = 30, .kind = ALARM_KIND_EVERYDAY, .is_smart = true});
  s_sleep_state = ActivitySleepStateAwake;
  s_sleep_state_seconds = 0;
  s_last_vmc = 0;
  prv_set_time(s_current_day, 10, 0);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  alarm_set_snooze_alarm();
  TimerID timer_id = s_alarm_snooze_timer_id;
  alarm_set_smart(id, false);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(alarm_get_most_recent_id(), ALARM_INVALID_ID);

  prv_set_time(s_current_day, 10, alarm_get_snooze_delay());
  stub_new_timer_invoke(1);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 1);
  prv_set_time(s_current_day, 10, 30);
  pbl_cron_wakeup();
  cl_assert_equal_i(s_num_alarm_events_put, 2);
  cl_assert_equal_i(alarm_get_most_recent_id(), id);
}
