/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/cron/cron.h>
#include <pbl/fs/fs.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/system_task.h>

#include <applib/ui/action_toggle.h>
#include <clar.h>
#include <kernel/events.h>
#include <resource/resource.h>

// Stubs
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <fake_new_timer.h>
#include <fake_pbl_malloc.h>
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>

#define PREF_KEY_DND_MANUALLY_ENABLED "dndManuallyEnabled"

static int s_num_dnd_events_put = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Fakes

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_DO_NOT_DISTURB_EVENT) {
    s_num_dnd_events_put++;
  }
}

// Thursday, March 12, 2015, 00:00 UTC
static const int s_thursday_00_00 = 1426118400;
// Thursday, March 12, 2015, 01:00 UTC
static const int s_thursday_01_00 = 1426122000;
// Thursday, March 12, 2015, 12:00 UTC
static const int s_thursday_12_00 = 1426161600;
// Thursday, March 12, 2015, 13:00 UTC
static const int s_thursday_13_00 = 1426165200;
// Friday, March 20, 2015, 12:00 UTC
static const int s_friday_08_30 = 1426840200;
// Friday, March 20, 2015, 23:30 UTC
static const int s_friday_23_30 = 1426894200;
// Saturday, March 21, 2015, 00:00 UTC
static const int s_saturday_00_00 = 1426896000;
// Saturday, March 21, 2015, 00:30 UTC
static const int s_saturday_00_30 = 1426897800;
// Saturday, March 21, 2015, 01:30 UTC
static const int s_saturday_01_30 = 1426901400;
// Saturday, March 21, 2015, 10:30 UTC
static const int s_saturday_10_30 = 1426933800;
// Sunday, March 22, 2015, 9:30 UTC
static const int s_sunday_9_30 = 1427016600;
// Sunday, March 22, 2015, 10:00 UTC
static const int s_sunday_10_00 = 1427018400;
// Sunday, March 22, 2015, 23:30 UTC
static const int s_sunday_23_30 = 1427067000;
// Monday, March 23, 2015, 10:30 UTC
static const int s_monday_10_30 = 1427106600;

static TimerID s_timer = TIMER_INVALID_ID;
static bool s_event_ongoing = false;

void calendar_init() {
  s_timer = new_timer_create();
}

bool calendar_event_is_ongoing() {
  return s_event_ongoing;
}

void do_not_disturb_toggle_push(ActionTogglePrompt prompt, bool set_exit_reason) {
  do_not_disturb_set_manually_enabled(!do_not_disturb_is_active());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Helper Functions

static void prv_assert_settings_value(const void *key, size_t key_len, const void *expected_value,
                                      size_t value_len) {
  SettingsFile file;
  char buffer[value_len];
  cl_must_pass(settings_file_open(&file, "/notifpref", 1024));
  cl_must_pass(settings_file_get(&file, key, key_len, buffer, value_len));
  settings_file_close(&file);
  cl_assert_equal_m(expected_value, buffer, value_len);
}

static void prv_assert_seconds_until_update(time_t expected) {
  cl_assert_equal_i(pbl_cron_get_next_execute_time() - rtc_get_time(), expected);
}

static void prv_assert_manually_dnd_setting_val(bool expected_value) {
  const char *key = "dndManuallyEnabled";
  prv_assert_settings_value((void *)key, strlen("dndManuallyEnabled"), (void *)&expected_value,
                            sizeof(bool));
}

//! Slots standing in for the weekday and weekend schedules
enum {
  WeekdaySlot,
  WeekendSlot,
};

static DoNotDisturbScheduleSlot prv_get_slot(unsigned int index) {
  DoNotDisturbScheduleSlot slot;
  do_not_disturb_get_schedule_slot(index, &slot);
  return slot;
}

static void prv_store_slot(unsigned int index, DoNotDisturbScheduleSlot slot) {
  if (!slot.used) {
    slot.kind = (index == WeekdaySlot) ? DoNotDisturbScheduleKindWeekdays
                                       : DoNotDisturbScheduleKindWeekends;
  }
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(index, &slot), 0);
}

static void prv_set_schedule(unsigned int index, const DoNotDisturbSchedule *schedule) {
  DoNotDisturbScheduleSlot slot = prv_get_slot(index);
  slot.schedule = *schedule;
  prv_store_slot(index, slot);
}

static void prv_set_schedule_enabled(unsigned int index, bool enabled) {
  DoNotDisturbScheduleSlot slot = prv_get_slot(index);
  slot.enabled = enabled;
  prv_store_slot(index, slot);
}

static void prv_toggle_schedule(unsigned int index) {
  prv_set_schedule_enabled(index, !prv_get_slot(index).enabled);
}

static bool prv_is_schedule_enabled(unsigned int index) {
  return prv_get_slot(index).enabled;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_do_not_disturb__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");

  rtc_set_time(s_thursday_00_00);
  alerts_preferences_init();
  pbl_cron_init();
  do_not_disturb_init();

  do_not_disturb_set_manually_enabled(false);
  for (int i = 0; i < DND_NUM_SCHEDULE_SLOTS; i++) {
    do_not_disturb_delete_schedule_slot(i);
  }
  if (do_not_disturb_is_smart_dnd_enabled()) {
    do_not_disturb_toggle_smart_dnd();
  }

  alerts_preferences_check_and_set_first_use_complete(FirstUseSourceSmartDND);

  s_event_ongoing = false;
  s_num_dnd_events_put = 0;
}

void test_do_not_disturb__cleanup(void) {
  // Make sure we start in a common state: everything off
  do_not_disturb_set_manually_enabled(false);
  for (int i = 0; i < DND_NUM_SCHEDULE_SLOTS; i++) {
    do_not_disturb_delete_schedule_slot(i);
  }
  pbl_cron_deinit();
}

void test_do_not_disturb__manually_enable(void) {
  cl_assert(do_not_disturb_is_active() == false);
  bool enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == false);

  do_not_disturb_set_manually_enabled(true);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == true);
  cl_assert(do_not_disturb_is_active() == true);
  prv_assert_manually_dnd_setting_val(true);
  cl_assert_equal_i(s_num_dnd_events_put, 1);

  do_not_disturb_set_manually_enabled(false);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert(enabled == false);
  prv_assert_manually_dnd_setting_val(false);
  cl_assert_equal_i(s_num_dnd_events_put, 2);

  do_not_disturb_set_manually_enabled(true);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(do_not_disturb_is_active() == true);
  cl_assert(enabled == true);
  prv_assert_manually_dnd_setting_val(true);
  cl_assert_equal_i(s_num_dnd_events_put, 3);
}

//! Simulate the phone writing a pref through settings sync: the record lands in the
//! settings file first, then the BlobDB insert event is handed to alerts_preferences.
static void prv_sync_bool_pref_from_phone(const char *key, bool value) {
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "/notifpref", 1024));
  cl_must_pass(settings_file_set(&file, key, strlen(key), &value, sizeof(value)));
  settings_file_close(&file);

  PebbleBlobDBEvent event = {
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = strlen(key),
  };
  alerts_preferences_handle_blob_db_event(&event);
}

void test_do_not_disturb__manual_pref_synced_from_phone(void) {
  cl_assert(do_not_disturb_is_active() == false);
  s_num_dnd_events_put = 0;

  prv_sync_bool_pref_from_phone(PREF_KEY_DND_MANUALLY_ENABLED, true);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(do_not_disturb_is_active() == true);
  cl_assert_equal_i(s_num_dnd_events_put, 1);

  prv_sync_bool_pref_from_phone(PREF_KEY_DND_MANUALLY_ENABLED, false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 2);
}

void test_do_not_disturb__manually_enable_active(void) {
  bool active;
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  do_not_disturb_set_manually_enabled(true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  do_not_disturb_set_manually_enabled(false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
}

void test_do_not_disturb__is_active(void) {
  // Time 00:00, Manual and Scheduled DND both OFF

  bool active;
  // !Manual && !Scheduled && !Smart
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == false);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // Manual && !Scheduled && !Smart
  do_not_disturb_set_manually_enabled(true);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == false);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Manual && Scheduled && !Smart
  prv_set_schedule_enabled(WeekdaySlot, true);
  DoNotDisturbSchedule schedule = {
    .from_hour = 0,
    .from_minute = 0,
    .to_hour = 11,
    .to_minute = 30,
  };
  prv_set_schedule(WeekdaySlot, &schedule);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && Scheduled && !Smart

  do_not_disturb_set_manually_enabled(false);
  cl_assert(do_not_disturb_is_active() == false);
  // Disabling manual DND overrides the active schedule; toggling it off and on clears that
  prv_toggle_schedule(WeekdaySlot);
  cl_assert(do_not_disturb_is_active() == false);
  prv_toggle_schedule(WeekdaySlot);
  cl_assert(do_not_disturb_is_active() == true);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Enabling Smart DND
  do_not_disturb_set_manually_enabled(false);
  prv_set_schedule_enabled(WeekdaySlot, false);
  calendar_init();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  do_not_disturb_toggle_smart_dnd();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == true);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  s_event_ongoing = true;

  // Manual && !Scheduled && Smart
  do_not_disturb_set_manually_enabled(true);
  prv_set_schedule_enabled(WeekdaySlot, false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Manual && Scheduled && Smart
  do_not_disturb_set_manually_enabled(true);
  prv_set_schedule_enabled(WeekdaySlot, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && !Scheduled && Smart
  do_not_disturb_set_manually_enabled(false);   // Overrides all DND and disables
  prv_set_schedule_enabled(WeekdaySlot, false); // Clears overrides
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && Scheduled && Smart
  do_not_disturb_set_manually_enabled(false);
  prv_set_schedule_enabled(WeekdaySlot, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);
}

void test_do_not_disturb__disabling_manual_dnd_should_override_scheduled(void) {
  bool active;
  // Time 00:00, Manual and Scheduled DND both OFF
  DoNotDisturbSchedule schedule = {
    .from_hour = 0,
    .from_minute = 30,
    .to_hour = 12,
    .to_minute = 30,
  };
  active = do_not_disturb_is_active();
  cl_assert(active == false); // both OFF

  prv_set_schedule(WeekdaySlot, &schedule);
  prv_set_schedule_enabled(WeekdaySlot, true);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // not in Scheduled mode

  rtc_set_time(s_thursday_01_00);
  do_not_disturb_handle_clock_change(); // In scheduled period

  do_not_disturb_set_manually_enabled(true); // both ON
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Both OFF

  do_not_disturb_set_manually_enabled(false); // turned Manual OFF, scheduled should be overriden
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Both OFF
}

void test_do_not_disturb__disable_manual_dnd_when_scheduled_ends(void) {
  bool active;
  // Time 00:00, Manual and Scheduled DND both OFF
  DoNotDisturbSchedule schedule = {
    .from_hour = 1,
    .from_minute = 0,
    .to_hour = 12,
    .to_minute = 30,
  };
  prv_set_schedule(WeekdaySlot, &schedule);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  do_not_disturb_set_manually_enabled(true);
  prv_set_schedule_enabled(WeekdaySlot, true);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // ON due to manual only

  do_not_disturb_set_manually_enabled(false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Both OFF

  do_not_disturb_set_manually_enabled(true);
  prv_set_schedule_enabled(WeekdaySlot, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Both ON

  rtc_set_time(s_thursday_12_00);
  do_not_disturb_handle_clock_change(); // In scheduled period
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Both ON

  rtc_set_time(s_thursday_13_00);
  do_not_disturb_handle_clock_change(); // Out of scheduled period
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Both should be turned off
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
}

void test_do_not_disturb__cron_fires_schedule_boundaries(void) {
  DoNotDisturbSchedule schedule = {
    .from_hour = 1,
    .from_minute = 0,
    .to_hour = 12,
    .to_minute = 30,
  };
  prv_set_schedule(WeekdaySlot, &schedule);
  prv_set_schedule_enabled(WeekdaySlot, true);
  cl_assert(do_not_disturb_is_active() == false);
  prv_assert_seconds_until_update(3600);

  rtc_set_time(s_thursday_01_00);
  pbl_cron_wakeup();
  cl_assert(do_not_disturb_is_active() == true);
  prv_assert_seconds_until_update(11.5 * PBL_SEC_PER_HOUR);

  do_not_disturb_set_manually_enabled(true);
  rtc_set_time(s_thursday_13_00);
  pbl_cron_wakeup();
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  prv_assert_seconds_until_update(12 * PBL_SEC_PER_HOUR);
}

void test_do_not_disturb__change_schedule_while_in_scheduled(void) {
  bool active;
  // Time 00:00, Manual and Scheduled DND both OFF
  DoNotDisturbSchedule schedule_1 = {
    .from_hour = 0,
    .from_minute = 0,
    .to_hour = 12,
    .to_minute = 30,
  };

  prv_set_schedule(WeekdaySlot, &schedule_1);
  prv_set_schedule_enabled(WeekdaySlot, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Scheduled ON

  DoNotDisturbSchedule schedule_2 = {
    .from_hour = 5,
    .from_minute = 0,
    .to_hour = 13,
    .to_minute = 0,
  };
  prv_set_schedule(WeekdaySlot, &schedule_2);

  rtc_set_time(s_thursday_12_00);
  do_not_disturb_handle_clock_change(); // Should still be in scheduled period
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Scheduled ON

  DoNotDisturbSchedule schedule_3 = {
    .from_hour = 14,
    .from_minute = 0,
    .to_hour = 15,
    .to_minute = 0,
  };
  prv_set_schedule(WeekdaySlot, &schedule_3);

  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == true);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Scheduled ON
}

void test_do_not_disturb__smart_dnd(void) {
  bool active;

  calendar_init();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  do_not_disturb_toggle_smart_dnd(); // Only smart DND is on
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == true);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  s_event_ongoing = true;
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  s_event_ongoing = false;
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  s_event_ongoing = true;
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Testing the override capability
  do_not_disturb_set_manually_enabled(false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
}

void test_do_not_disturb__weekday_weekend_schedule(void) {
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == false);
  cl_assert(prv_is_schedule_enabled(WeekendSlot) == false);
  cl_assert(do_not_disturb_is_active() == false);

  // 11 PM - 7 AM, starting Monday to Friday
  const DoNotDisturbSchedule weekday_schedule = {
    .from_hour = 23,
    .to_hour = 7,
  };
  // 1 AM - 9 AM
  const DoNotDisturbSchedule weekend_schedule = {
    .from_hour = 1,
    .to_hour = 9,
  };

  prv_set_schedule(WeekdaySlot, &weekday_schedule);
  prv_set_schedule(WeekendSlot, &weekend_schedule);
  prv_set_schedule_enabled(WeekdaySlot, true);
  prv_set_schedule_enabled(WeekendSlot, true);

  rtc_set_time(s_friday_08_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Friday 23:00
  prv_assert_seconds_until_update(52200);

  rtc_set_time(s_friday_23_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 01:00
  prv_assert_seconds_until_update(5400);

  // Friday's period carries on into Saturday

  rtc_set_time(s_saturday_00_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 01:00
  prv_assert_seconds_until_update(1800);

  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 07:00, end of Friday's period
  prv_assert_seconds_until_update(19800);

  rtc_set_time(s_saturday_10_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Sunday 01:00
  prv_assert_seconds_until_update(52200);

  prv_set_schedule_enabled(WeekendSlot, false);

  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 07:00, end of Friday's period
  prv_assert_seconds_until_update(19800);

  rtc_set_time(s_thursday_00_00);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Thursday 07:00
  prv_assert_seconds_until_update(25200);

  cl_assert(pbl_cron_get_job_count() != 0);
  prv_set_schedule_enabled(WeekdaySlot, false);
  cl_assert(do_not_disturb_is_active() == false);
  // Neither schedules enabled, nothing should be scheduled
  cl_assert_equal_i(pbl_cron_get_job_count(), 0);

  prv_set_schedule_enabled(WeekdaySlot, true);
  cl_assert(do_not_disturb_is_active() == true);
  // Thursday 07:00
  prv_assert_seconds_until_update(25200);

  prv_set_schedule_enabled(WeekendSlot, true);
  prv_set_schedule_enabled(WeekdaySlot, false);

  rtc_set_time(s_thursday_01_00);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Saturday 01:00
  prv_assert_seconds_until_update(172800);

  prv_set_schedule_enabled(WeekendSlot, false);
  prv_set_schedule_enabled(WeekdaySlot, true);

  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 07:00, end of Friday's period
  prv_assert_seconds_until_update(19800);

  // 10:30 PM - 8:30 AM
  const DoNotDisturbSchedule weekday_schedule_2 = {
    .from_hour = 22,
    .from_minute = 30,
    .to_hour = 8,
    .to_minute = 30,
  };
  // 12 AM - 10 AM
  const DoNotDisturbSchedule weekend_schedule_2 = {
    .from_hour = 0,
    .to_hour = 10,
  };

  prv_set_schedule(WeekdaySlot, &weekday_schedule_2);
  prv_set_schedule(WeekendSlot, &weekend_schedule_2);
  prv_set_schedule_enabled(WeekdaySlot, true);
  prv_set_schedule_enabled(WeekendSlot, true);

  rtc_set_time(s_friday_23_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 00:00
  prv_assert_seconds_until_update(1800);

  rtc_set_time(s_saturday_00_00);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Saturday 08:30, end of Friday's period
  prv_assert_seconds_until_update(30600);

  rtc_set_time(s_saturday_10_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Sunday 00:00
  prv_assert_seconds_until_update(48600);

  rtc_set_time(s_sunday_9_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == true);
  // Sunday 10:00
  prv_assert_seconds_until_update(1800);

  rtc_set_time(s_sunday_10_00);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Monday 22:30
  prv_assert_seconds_until_update(131400);

  // No period starts on Sunday night

  rtc_set_time(s_sunday_23_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Monday 22:30
  prv_assert_seconds_until_update(82800);

  rtc_set_time(s_monday_10_30);
  do_not_disturb_handle_clock_change();
  cl_assert(do_not_disturb_is_active() == false);
  // Monday 22:30
  prv_assert_seconds_until_update(43200);
}

void test_do_not_disturb__toggle_manually_enabled(void) {
  bool active, enabled;

  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(prv_is_schedule_enabled(WeekdaySlot) == false);
  cl_assert(prv_is_schedule_enabled(WeekendSlot) == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // First toggles are no-ops in unit tests, because the logic that handles the first time tutorial
  // dialog are stubbed out
  //////////////////////////////////////////////////////////////////////////////////////////
  do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceSettingsMenu);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceActionMenu);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  //////////////////////////////////////////////////////////////////////////////////////////

  do_not_disturb_toggle_smart_dnd();
  s_event_ongoing = true;
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Toggles only the manual DND setting. If set to off, overrides Smart and Scheduled DND
  do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceSettingsMenu);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  do_not_disturb_set_manually_enabled(false);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // Reset override and enable Smart DND
  do_not_disturb_toggle_smart_dnd();
  do_not_disturb_toggle_smart_dnd();
  s_event_ongoing = true;
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Does not necessarily toggle Manual DND, sets Manual DND to opposite of DND active status and
  // overrides Smart and Scheduled DND
  do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceActionMenu);
  enabled = do_not_disturb_is_manually_enabled();
  cl_assert(enabled == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
}

// Wednesday, March 11, 2015, 02:00 UTC
static const int s_wednesday_02_00 = 1426039200;
// Thursday, March 12, 2015, 02:00 UTC
static const int s_thursday_02_00 = 1426125600;

static void prv_write_pref(const char *key, const void *value, size_t value_len) {
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "/notifpref", 1024));
  cl_must_pass(settings_file_set(&file, key, strlen(key), value, value_len));
  settings_file_close(&file);
}

static void prv_delete_pref(const char *key) {
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "/notifpref", 1024));
  settings_file_delete(&file, key, strlen(key));
  settings_file_close(&file);
}

static void prv_sync_pref_from_phone(const char *key, const void *value, size_t value_len) {
  prv_write_pref(key, value, value_len);
  PebbleBlobDBEvent event = {
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = strlen(key),
  };
  alerts_preferences_handle_blob_db_event(&event);
}

static void prv_set_time(time_t t) {
  rtc_set_time(t);
  do_not_disturb_handle_clock_change();
}

void test_do_not_disturb__add_and_delete_slots(void) {
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 1, .to_hour = 2},
  };
  for (int i = 0; i < DND_NUM_SCHEDULE_SLOTS; i++) {
    cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), i);
    cl_assert(prv_get_slot(i).used);
  }
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), -ENOSPC);

  do_not_disturb_delete_schedule_slot(2);
  cl_assert(!prv_get_slot(2).used);
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), 2);

  for (int i = 0; i < DND_NUM_SCHEDULE_SLOTS; i++) {
    do_not_disturb_delete_schedule_slot(i);
  }
  cl_assert_equal_i(pbl_cron_get_job_count(), 0);
}

void test_do_not_disturb__invalid_slots_rejected(void) {
  const DoNotDisturbScheduleSlot invalid[] = {
    {.kind = DoNotDisturbScheduleKindCount},
    {.kind = DoNotDisturbScheduleKindCustom},
    {.kind = DoNotDisturbScheduleKindCustom, .days = 0x80},
    {.kind = DoNotDisturbScheduleKindEveryday, .schedule = {.from_hour = 24}},
    {.kind = DoNotDisturbScheduleKindEveryday, .schedule = {.to_minute = 60}},
  };
  for (unsigned int i = 0; i < ARRAY_LENGTH(invalid); i++) {
    cl_assert(!do_not_disturb_schedule_slot_is_valid(&invalid[i]));
    cl_assert_equal_i(do_not_disturb_add_schedule_slot(&invalid[i]), -EINVAL);
    cl_assert_equal_i(do_not_disturb_set_schedule_slot(0, &invalid[i]), -EINVAL);
  }
  const DoNotDisturbScheduleSlot valid = {.kind = DoNotDisturbScheduleKindEveryday};
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(DND_NUM_SCHEDULE_SLOTS, &valid), -EINVAL);
  cl_assert_equal_i(pbl_cron_get_job_count(), 0);
}

void test_do_not_disturb__custom_days(void) {
  // Tuesday and Thursday, 01:00 - 02:00
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindCustom,
    .days = PBL_CRON_WDAY_TUESDAY | PBL_CRON_WDAY_THURSDAY,
    .schedule = {.from_hour = 1, .to_hour = 2},
  };
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), 0);

  // Thursday 01:00
  prv_assert_seconds_until_update(PBL_SEC_PER_HOUR);
  prv_set_time(s_thursday_01_00);
  cl_assert(do_not_disturb_is_active());
  // Thursday 02:00
  prv_assert_seconds_until_update(PBL_SEC_PER_HOUR);

  // Next Tuesday 01:00
  prv_set_time(s_thursday_02_00);
  cl_assert(!do_not_disturb_is_active());
  prv_assert_seconds_until_update(4 * PBL_SEC_PER_DAY + 23 * PBL_SEC_PER_HOUR);

  prv_set_time(s_thursday_01_00 + PBL_SEC_PER_DAY);
  cl_assert(!do_not_disturb_is_active());
}

void test_do_not_disturb__period_spanning_midnight_belongs_to_start_day(void) {
  // Wednesday 22:00 - 07:00
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindCustom,
    .days = PBL_CRON_WDAY_WEDNESDAY,
    .schedule = {.from_hour = 22, .to_hour = 7},
  };
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), 0);

  prv_set_time(s_wednesday_02_00);
  cl_assert(!do_not_disturb_is_active());
  // Wednesday 22:00
  prv_assert_seconds_until_update(20 * PBL_SEC_PER_HOUR);

  prv_set_time(s_thursday_02_00);
  cl_assert(do_not_disturb_is_active());
  // Thursday 07:00
  prv_assert_seconds_until_update(5 * PBL_SEC_PER_HOUR);
}

void test_do_not_disturb__weekdays_period_spanning_midnight(void) {
  // Weekdays 22:00 - 07:00: Friday night is quiet, Sunday night is not
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 22, .to_hour = 7},
  };
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), 0);

  prv_set_time(s_saturday_01_30);
  cl_assert(do_not_disturb_is_active());
  prv_set_time(s_sunday_23_30);
  cl_assert(!do_not_disturb_is_active());
  prv_set_time(s_monday_10_30 - 9 * PBL_SEC_PER_HOUR);
  cl_assert(!do_not_disturb_is_active());
}

void test_do_not_disturb__equal_start_and_end_never_active(void) {
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 1, .to_hour = 1},
  };
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&slot), 0);

  prv_set_time(s_thursday_01_00);
  cl_assert(!do_not_disturb_is_active());
  cl_assert_equal_i(pbl_cron_get_job_count(), 0);
}

void test_do_not_disturb__overlapping_slots(void) {
  const DoNotDisturbScheduleSlot first = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 0, .to_hour = 2},
  };
  const DoNotDisturbScheduleSlot second = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 1, .to_hour = 13},
  };
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&first), 0);
  cl_assert_equal_i(do_not_disturb_add_schedule_slot(&second), 1);

  prv_set_time(s_thursday_02_00);
  cl_assert(do_not_disturb_is_active());

  do_not_disturb_delete_schedule_slot(1);
  cl_assert(!do_not_disturb_is_active());
}

typedef struct {
  DoNotDisturbSchedule schedule;
  bool enabled;
} LegacySchedule;

static void prv_reinit_with_legacy(const LegacySchedule *weekday, const LegacySchedule *weekend) {
  prv_write_pref("dndWeekdaySchedule", &weekday->schedule, sizeof(weekday->schedule));
  prv_write_pref("dndWeekdayScheduleEnabled", &weekday->enabled, sizeof(weekday->enabled));
  prv_write_pref("dndWeekendSchedule", &weekend->schedule, sizeof(weekend->schedule));
  prv_write_pref("dndWeekendScheduleEnabled", &weekend->enabled, sizeof(weekend->enabled));
  prv_delete_pref("qtSchedule0");
  prv_delete_pref("qtSchedule1");
  alerts_preferences_init();
  do_not_disturb_handle_clock_change();
}

void test_do_not_disturb__migrate_legacy_schedules(void) {
  const LegacySchedule weekday = {
    .schedule = {.from_hour = 22, .to_hour = 7},
    .enabled = true,
  };
  const LegacySchedule weekend = {
    .schedule = {.from_hour = 10, .to_hour = 12},
    .enabled = false,
  };
  prv_reinit_with_legacy(&weekday, &weekend);

  DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  cl_assert(slot.used && slot.enabled && slot.legacy);
  cl_assert_equal_i(slot.kind, DoNotDisturbScheduleKindWeekdays);
  cl_assert_equal_i(slot.schedule.from_hour, 22);
  cl_assert_equal_i(slot.schedule.to_hour, 7);

  slot = prv_get_slot(WeekendSlot);
  cl_assert(slot.used && !slot.enabled && slot.legacy);
  cl_assert_equal_i(slot.kind, DoNotDisturbScheduleKindWeekends);
  cl_assert_equal_i(slot.schedule.from_hour, 10);

  // Every weekday is quiet in the evening and early morning, as before
  prv_set_time(s_friday_23_30);
  cl_assert(do_not_disturb_is_active());
  // Saturday 00:00
  prv_assert_seconds_until_update(30 * PBL_SEC_PER_MIN);
  prv_set_time(s_saturday_01_30);
  cl_assert(!do_not_disturb_is_active());
  prv_set_time(s_sunday_23_30);
  cl_assert(!do_not_disturb_is_active());
  // Monday 00:00
  prv_assert_seconds_until_update(30 * PBL_SEC_PER_MIN);
  prv_set_time(s_monday_10_30 - 9 * PBL_SEC_PER_HOUR);
  cl_assert(do_not_disturb_is_active());
}

void test_do_not_disturb__migrate_default_legacy_schedules(void) {
  const LegacySchedule defaults = {
    .schedule = {.from_hour = 0, .to_hour = 6},
  };
  prv_reinit_with_legacy(&defaults, &defaults);

  cl_assert(!prv_get_slot(WeekdaySlot).used);
  cl_assert(!prv_get_slot(WeekendSlot).used);
}

void test_do_not_disturb__migrate_legacy_schedules_once(void) {
  const LegacySchedule defaults = {
    .schedule = {.from_hour = 0, .to_hour = 6},
  };
  const LegacySchedule weekend = {
    .schedule = {.from_hour = 10, .to_hour = 12},
    .enabled = true,
  };
  prv_reinit_with_legacy(&defaults, &weekend);
  cl_assert(prv_get_slot(WeekendSlot).used);

  do_not_disturb_delete_schedule_slot(WeekendSlot);
  alerts_preferences_init();
  cl_assert(!prv_get_slot(WeekendSlot).used);
}

void test_do_not_disturb__migrate_invalid_legacy_schedule(void) {
  const LegacySchedule weekday = {
    .schedule = {.from_hour = 24, .to_hour = 7},
    .enabled = true,
  };
  const LegacySchedule defaults = {
    .schedule = {.from_hour = 0, .to_hour = 6},
  };
  prv_reinit_with_legacy(&weekday, &defaults);

  const DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  cl_assert(slot.used && slot.enabled && slot.legacy);
  cl_assert_equal_m(&defaults.schedule, &slot.schedule, sizeof(slot.schedule));
  prv_assert_settings_value("dndWeekdaySchedule", strlen("dndWeekdaySchedule"), &defaults.schedule,
                            sizeof(defaults.schedule));

  alerts_preferences_init();
  cl_assert(prv_get_slot(WeekdaySlot).used);
}

void test_do_not_disturb__invalid_stored_slot_dropped(void) {
  const DoNotDisturbScheduleSlot invalid = {
    .used = true,
    .kind = DoNotDisturbScheduleKindCount,
  };
  prv_write_pref("qtSchedule3", &invalid, sizeof(invalid));
  alerts_preferences_init();
  cl_assert(!prv_get_slot(3).used);

  const uint8_t short_record[3] = {1, 1, 0};
  prv_write_pref("qtSchedule3", short_record, sizeof(short_record));
  alerts_preferences_init();
  cl_assert(!prv_get_slot(3).used);
}

void test_do_not_disturb__slot_synced_from_phone(void) {
  const DoNotDisturbScheduleSlot slot = {
    .used = true,
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 0, .to_hour = 2},
  };
  prv_sync_pref_from_phone("qtSchedule2", &slot, sizeof(slot));
  cl_assert(prv_get_slot(2).used);
  cl_assert(do_not_disturb_is_active());

  prv_sync_pref_from_phone("qtSchedule2", &(DoNotDisturbScheduleSlot){}, sizeof(slot));
  cl_assert(!prv_get_slot(2).used);
  cl_assert(!do_not_disturb_is_active());
}

void test_do_not_disturb__invalid_slot_from_phone_reverted(void) {
  const DoNotDisturbScheduleSlot slot = {
    .used = true,
    .enabled = true,
    .kind = DoNotDisturbScheduleKindEveryday,
    .schedule = {.from_hour = 0, .to_hour = 2},
  };
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(2, &slot), 0);

  const DoNotDisturbScheduleSlot invalid = {
    .used = true,
    .enabled = true,
    .kind = 7,
    .schedule = {.from_hour = 25},
  };
  prv_sync_pref_from_phone("qtSchedule2", &invalid, sizeof(invalid));
  DoNotDisturbScheduleSlot stored = prv_get_slot(2);
  cl_assert_equal_m(&slot, &stored, sizeof(slot));
  prv_assert_settings_value("qtSchedule2", strlen("qtSchedule2"), &slot, sizeof(slot));

  const uint8_t short_record[3] = {1, 1, 0};
  prv_sync_pref_from_phone("qtSchedule2", short_record, sizeof(short_record));
  stored = prv_get_slot(2);
  cl_assert_equal_m(&slot, &stored, sizeof(slot));
  prv_assert_settings_value("qtSchedule2", strlen("qtSchedule2"), &slot, sizeof(slot));
}

void test_do_not_disturb__legacy_phone_write_fills_slot_when_enabled(void) {
  const DoNotDisturbSchedule schedule = {.from_hour = 0, .to_hour = 2};
  prv_sync_pref_from_phone("dndWeekdaySchedule", &schedule, sizeof(schedule));
  cl_assert(!prv_get_slot(WeekdaySlot).used);

  const bool disabled = false;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &disabled, sizeof(disabled));
  cl_assert(!prv_get_slot(WeekdaySlot).used);

  const bool enabled = true;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &enabled, sizeof(enabled));
  const DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  cl_assert(slot.used && slot.enabled && slot.legacy);
  cl_assert_equal_i(slot.kind, DoNotDisturbScheduleKindWeekdays);
  cl_assert_equal_i(slot.schedule.to_hour, 2);
  cl_assert(do_not_disturb_is_active());
}

static void prv_enable_legacy_weekday_from_phone(const DoNotDisturbSchedule *schedule) {
  prv_sync_pref_from_phone("dndWeekdaySchedule", schedule, sizeof(*schedule));
  const bool enabled = true;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &enabled, sizeof(enabled));
  cl_assert(prv_get_slot(WeekdaySlot).legacy);
}

void test_do_not_disturb__legacy_phone_write_applies_written_field(void) {
  prv_enable_legacy_weekday_from_phone(&(DoNotDisturbSchedule){.from_hour = 20, .to_hour = 21});

  // An enabled write keeps the slot's times
  const bool disabled = false;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &disabled, sizeof(disabled));
  DoNotDisturbScheduleSlot updated = prv_get_slot(WeekdaySlot);
  cl_assert(!updated.enabled);
  cl_assert_equal_i(updated.schedule.from_hour, 20);

  // A schedule write keeps the slot's enabled state and days
  const DoNotDisturbSchedule schedule = {.from_hour = 23, .to_hour = 6};
  prv_sync_pref_from_phone("dndWeekdaySchedule", &schedule, sizeof(schedule));
  updated = prv_get_slot(WeekdaySlot);
  cl_assert(!updated.enabled && updated.legacy);
  cl_assert_equal_i(updated.schedule.from_hour, 23);
  cl_assert_equal_i(updated.kind, DoNotDisturbScheduleKindWeekdays);
}

void test_do_not_disturb__legacy_phone_write_of_same_times_keeps_coverage(void) {
  const DoNotDisturbSchedule schedule = {.from_hour = 22, .to_hour = 7};
  prv_enable_legacy_weekday_from_phone(&schedule);

  prv_set_time(s_friday_23_30);
  cl_assert(do_not_disturb_is_active());
  prv_sync_pref_from_phone("dndWeekdaySchedule", &schedule, sizeof(schedule));
  cl_assert(do_not_disturb_is_active());
  cl_assert_equal_i(prv_get_slot(WeekdaySlot).kind, DoNotDisturbScheduleKindWeekdays);
}

void test_do_not_disturb__legacy_phone_write_ignored_for_other_days(void) {
  // Made on the watch: not a legacy slot, even with the same days
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 20, .to_hour = 21},
  };
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(WeekdaySlot, &slot), 0);

  const bool disabled = false;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &disabled, sizeof(disabled));
  const DoNotDisturbSchedule schedule = {.from_hour = 1, .to_hour = 2};
  prv_sync_pref_from_phone("dndWeekdaySchedule", &schedule, sizeof(schedule));

  const DoNotDisturbScheduleSlot expected = {
    .used = true,
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 20, .to_hour = 21},
  };
  const DoNotDisturbScheduleSlot stored = prv_get_slot(WeekdaySlot);
  cl_assert_equal_m(&expected, &stored, sizeof(expected));
}

void test_do_not_disturb__legacy_phone_enable_reverted_for_other_slots(void) {
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 20, .to_hour = 21},
  };
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(WeekdaySlot, &slot), 0);

  const bool enabled = true;
  prv_sync_pref_from_phone("dndWeekdayScheduleEnabled", &enabled, sizeof(enabled));
  const bool disabled = false;
  prv_assert_settings_value("dndWeekdayScheduleEnabled", strlen("dndWeekdayScheduleEnabled"),
                            &disabled, sizeof(disabled));
  cl_assert(!prv_get_slot(WeekdaySlot).legacy);
}

void test_do_not_disturb__invalid_legacy_phone_write_reverted(void) {
  const DoNotDisturbSchedule schedule = {.from_hour = 20, .to_hour = 21};
  prv_enable_legacy_weekday_from_phone(&schedule);

  const DoNotDisturbSchedule invalid = {.from_hour = 24, .to_hour = 7};
  prv_sync_pref_from_phone("dndWeekdaySchedule", &invalid, sizeof(invalid));
  cl_assert_equal_i(prv_get_slot(WeekdaySlot).schedule.from_hour, 20);
  prv_assert_settings_value("dndWeekdaySchedule", strlen("dndWeekdaySchedule"), &schedule,
                            sizeof(schedule));

  // An unused slot falls back to the default schedule
  prv_sync_pref_from_phone("dndWeekendSchedule", &invalid, sizeof(invalid));
  const bool enabled = true;
  prv_sync_pref_from_phone("dndWeekendScheduleEnabled", &enabled, sizeof(enabled));
  const DoNotDisturbScheduleSlot weekend = prv_get_slot(WeekendSlot);
  cl_assert(weekend.used && weekend.legacy);
  cl_assert_equal_i(weekend.schedule.from_hour, 0);
  cl_assert_equal_i(weekend.schedule.to_hour, 6);
}

void test_do_not_disturb__phone_slot_write_updates_legacy_schedule(void) {
  prv_enable_legacy_weekday_from_phone(&(DoNotDisturbSchedule){.from_hour = 20, .to_hour = 21});

  DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  slot.schedule = (DoNotDisturbSchedule){.from_hour = 8, .to_hour = 9};
  prv_sync_pref_from_phone("qtSchedule0", &slot, sizeof(slot));
  prv_assert_settings_value("dndWeekdaySchedule", strlen("dndWeekdaySchedule"), &slot.schedule,
                            sizeof(slot.schedule));

  prv_sync_pref_from_phone("qtSchedule0", &(DoNotDisturbScheduleSlot){}, sizeof(slot));
  const bool disabled = false;
  prv_assert_settings_value("dndWeekdayScheduleEnabled", strlen("dndWeekdayScheduleEnabled"),
                            &disabled, sizeof(disabled));
}

void test_do_not_disturb__legacy_only_in_first_slots(void) {
  const DoNotDisturbScheduleSlot slot = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 20, .to_hour = 21},
    .legacy = true,
  };
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(2, &slot), 0);
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(2, &slot), 0);
  cl_assert(!prv_get_slot(2).legacy);

  DoNotDisturbScheduleSlot phone = prv_get_slot(2);
  phone.legacy = true;
  prv_sync_pref_from_phone("qtSchedule2", &phone, sizeof(phone));
  cl_assert(!prv_get_slot(2).legacy);
}

void test_do_not_disturb__unused_phone_slot_zeroed(void) {
  const DoNotDisturbScheduleSlot unused = {
    .enabled = true,
    .kind = DoNotDisturbScheduleKindWeekdays,
    .schedule = {.from_hour = 20, .to_hour = 21},
  };
  prv_sync_pref_from_phone("qtSchedule3", &unused, sizeof(unused));
  const DoNotDisturbScheduleSlot zero = {};
  DoNotDisturbScheduleSlot stored = prv_get_slot(3);
  cl_assert_equal_m(&zero, &stored, sizeof(zero));

  alerts_preferences_init();
  stored = prv_get_slot(3);
  cl_assert_equal_m(&zero, &stored, sizeof(zero));
}

void test_do_not_disturb__invalid_stored_legacy_slot_disables_legacy_schedule(void) {
  prv_enable_legacy_weekday_from_phone(&(DoNotDisturbSchedule){.from_hour = 20, .to_hour = 21});

  DoNotDisturbScheduleSlot invalid = prv_get_slot(WeekdaySlot);
  invalid.kind = DoNotDisturbScheduleKindCount;
  prv_write_pref("qtSchedule0", &invalid, sizeof(invalid));
  alerts_preferences_init();

  cl_assert(!prv_get_slot(WeekdaySlot).used);
  const bool disabled = false;
  prv_assert_settings_value("dndWeekdayScheduleEnabled", strlen("dndWeekdayScheduleEnabled"),
                            &disabled, sizeof(disabled));
}

void test_do_not_disturb__slot_edit_updates_legacy_schedule(void) {
  prv_enable_legacy_weekday_from_phone(&(DoNotDisturbSchedule){.from_hour = 20, .to_hour = 21});

  // New times keep the slot legacy and reach the legacy schedule
  DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  slot.schedule = (DoNotDisturbSchedule){.from_hour = 8, .to_hour = 9};
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(WeekdaySlot, &slot), 0);
  cl_assert(prv_get_slot(WeekdaySlot).legacy);
  const bool enabled = true;
  prv_assert_settings_value("dndWeekdaySchedule", strlen("dndWeekdaySchedule"), &slot.schedule,
                            sizeof(slot.schedule));
  prv_assert_settings_value("dndWeekdayScheduleEnabled", strlen("dndWeekdayScheduleEnabled"),
                            &enabled, sizeof(enabled));

  // New days detach it, and the legacy schedule turns off
  slot.kind = DoNotDisturbScheduleKindEveryday;
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(WeekdaySlot, &slot), 0);
  cl_assert(!prv_get_slot(WeekdaySlot).legacy);
  const bool disabled = false;
  prv_assert_settings_value("dndWeekdayScheduleEnabled", strlen("dndWeekdayScheduleEnabled"),
                            &disabled, sizeof(disabled));

  slot.kind = DoNotDisturbScheduleKindWeekdays;
  cl_assert_equal_i(do_not_disturb_set_schedule_slot(WeekdaySlot, &slot), 0);
  cl_assert(!prv_get_slot(WeekdaySlot).legacy);
}

void test_do_not_disturb__phone_slot_write_with_new_days_clears_legacy(void) {
  prv_enable_legacy_weekday_from_phone(&(DoNotDisturbSchedule){.from_hour = 20, .to_hour = 21});

  DoNotDisturbScheduleSlot slot = prv_get_slot(WeekdaySlot);
  slot.kind = DoNotDisturbScheduleKindEveryday;
  prv_sync_pref_from_phone("qtSchedule0", &slot, sizeof(slot));

  slot.legacy = false;
  const DoNotDisturbScheduleSlot stored = prv_get_slot(WeekdaySlot);
  cl_assert_equal_m(&slot, &stored, sizeof(slot));
  prv_assert_settings_value("qtSchedule0", strlen("qtSchedule0"), &slot, sizeof(slot));
}
