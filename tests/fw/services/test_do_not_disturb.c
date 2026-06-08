/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/do_not_disturb.h"

#include "applib/ui/action_toggle.h"
#include "kernel/events.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/blob_db/api_types.h"
#include "resource/resource.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/services/system_task.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "pbl/services/settings/settings_file.h"

#include <stdint.h>
#include <string.h>

#include "clar.h"

// Stubs
#include "stubs_alerts.h"
#include "stubs_analytics.h"
#include "stubs_app_state.h"
#include "stubs_dialog.h"
#include "stubs_event_service_client.h"
#include "stubs_expandable_dialog.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_simple_dialog.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_vibe_score_info.h"
#include "stubs_vibes.h"
#include "stubs_window_manager.h"

#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"

#define PREF_KEY_DND_MANUALLY_ENABLED "dndManuallyEnabled"

static int s_num_dnd_events_put = 0;
static int s_num_pref_change_events_put = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Fakes

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

void event_put(PebbleEvent* event) {
  // Count DND and pref events separately; the receive path posts both,
  // the local setter path only the DND event.
  if (event->type == PEBBLE_DO_NOT_DISTURB_EVENT) s_num_dnd_events_put++;
  else if (event->type == PEBBLE_PREF_CHANGE_EVENT) s_num_pref_change_events_put++;
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

static void prv_assert_settings_value(const void *key, size_t key_len,
                                      const void *expected_value, size_t value_len) {
  SettingsFile file;
  char buffer[value_len];
  cl_must_pass(settings_file_open(&file, "notifpref", 1024));
  cl_must_pass(settings_file_get(&file, key, key_len, buffer, value_len));
  settings_file_close(&file);
  cl_assert_equal_m(expected_value, buffer, value_len);
}

static void prv_assert_manually_dnd_setting_val(bool expected_value) {
  const char *key = "dndManuallyEnabled";
  prv_assert_settings_value((void*)key, strlen("dndManuallyEnabled"),
                            (void*)&expected_value, sizeof(bool));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_do_not_disturb__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);

  rtc_set_time(s_thursday_00_00);
  alerts_preferences_init();
  do_not_disturb_init();

  do_not_disturb_set_manually_enabled(false);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, false);
  if (do_not_disturb_is_smart_dnd_enabled()) {
    do_not_disturb_toggle_smart_dnd();
  }

  alerts_preferences_check_and_set_first_use_complete(FirstUseSourceSmartDND);

  s_event_ongoing = false;
  s_num_dnd_events_put = 0;
  s_num_pref_change_events_put = 0;
}

void test_do_not_disturb__cleanup(void) {
  // Make sure we start in a common state: everything off
  do_not_disturb_set_manually_enabled(false);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, false);
  set_dnd_timer_id(TIMER_INVALID_ID);
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
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == false);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // Manual && !Scheduled && !Smart
  do_not_disturb_set_manually_enabled(true);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == false);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Manual && Scheduled && !Smart
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  DoNotDisturbSchedule schedule = {
    .from_hour = 0,
    .from_minute = 0,
    .to_hour = 11,
    .to_minute = 30,
  };
  do_not_disturb_set_schedule(WeekdaySchedule, &schedule);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && Scheduled && !Smart

  do_not_disturb_set_manually_enabled(false);
  cl_assert(do_not_disturb_is_active() == false);
  do_not_disturb_toggle_scheduled(WeekdaySchedule); // see PBL-22011
  cl_assert(do_not_disturb_is_active() == false);
  do_not_disturb_toggle_scheduled(WeekdaySchedule); // see PBL-22011
  cl_assert(do_not_disturb_is_active() == true);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Enabling Smart DND
  do_not_disturb_set_manually_enabled(false);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  calendar_init();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  do_not_disturb_toggle_smart_dnd();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == true);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  s_event_ongoing = true;

  // Manual && !Scheduled && Smart
  do_not_disturb_set_manually_enabled(true);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Manual && Scheduled && Smart
  do_not_disturb_set_manually_enabled(true);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && !Scheduled && Smart
  do_not_disturb_set_manually_enabled(false); // Overrides all DND and disables
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false); // Clears overrides
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // !Manual && Scheduled && Smart
  do_not_disturb_set_manually_enabled(false);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
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

  do_not_disturb_set_schedule(WeekdaySchedule, &schedule);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // not in Scheduled mode

  rtc_set_time(s_thursday_01_00);
  do_not_disturb_handle_clock_change(); // In scheduled period

  do_not_disturb_set_manually_enabled(true); // both ON
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Both OFF

  do_not_disturb_set_manually_enabled(false); // turned Manual OFF, scheduled should be overriden
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
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
  do_not_disturb_set_schedule(WeekdaySchedule, &schedule);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  do_not_disturb_set_manually_enabled(true);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  cl_assert(do_not_disturb_is_manually_enabled() == true);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // ON due to manual only

  do_not_disturb_set_manually_enabled(false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Both OFF

  do_not_disturb_set_manually_enabled(true);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
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
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
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

  do_not_disturb_set_schedule(WeekdaySchedule, &schedule_1);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true); // Scheduled ON

  DoNotDisturbSchedule schedule_2 = {
    .from_hour = 5,
    .from_minute = 0,
    .to_hour = 13,
    .to_minute = 0,
  };
  do_not_disturb_set_schedule(WeekdaySchedule, &schedule_2);

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
  do_not_disturb_set_schedule(WeekdaySchedule, &schedule_3);

  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == true);
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
  bool active;

  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekendSchedule) == false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // 11 PM - 7 AM
  DoNotDisturbSchedule weekday_schedule = {
    .from_hour = 23,
    .from_minute = 0,
    .to_hour = 7,
    .to_minute = 0,
  };

  // 1 AM - 9 AM
  DoNotDisturbSchedule weekend_schedule = {
    .from_hour = 1,
    .from_minute = 0,
    .to_hour = 9,
    .to_minute = 0,
  };

  do_not_disturb_set_schedule(WeekdaySchedule, &weekday_schedule);
  do_not_disturb_set_schedule(WeekendSchedule, &weekend_schedule);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, true);

  rtc_set_time(s_friday_08_30); // Out of schedule
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 23:00 on Friday. (14.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 52200 * MS_PER_SECOND);

  rtc_set_time(s_friday_23_30); // In schedule
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 00:00 on Saturday. (0.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 1800 * MS_PER_SECOND);

  rtc_set_time(s_saturday_00_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 01:00 on Saturday. (0.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 1800 * MS_PER_SECOND);

  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 09:00 on Saturday. (7.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 27000 * MS_PER_SECOND);

  rtc_set_time(s_saturday_10_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 01:00 on Sunday. (14.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 52200 * MS_PER_SECOND);

  do_not_disturb_set_schedule_enabled(WeekendSchedule, false);
  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 00:00 on Monday. (46.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 167400 * MS_PER_SECOND);

  rtc_set_time(s_thursday_00_00);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 07:00 on Thursday. (7.0 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 25200 * MS_PER_SECOND);

  // Check that there is a timer scheduled
  cl_assert(stub_new_timer_is_scheduled(get_dnd_timer_id()));
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Neither schedules enabled, timer should not be scheduled
  cl_assert(!stub_new_timer_is_scheduled(get_dnd_timer_id()));

  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 07:00 on Thursday. (7.0 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 25200 * MS_PER_SECOND);

  do_not_disturb_set_schedule_enabled(WeekendSchedule, true);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  rtc_set_time(s_thursday_01_00);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 00:00 on Saturday. (47.0 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 169200 * MS_PER_SECOND);

  do_not_disturb_set_schedule_enabled(WeekendSchedule, false);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  rtc_set_time(s_saturday_01_30);
  do_not_disturb_handle_clock_change();
  cl_assert(active == false);
  // Timer will go off at 00:00 on Saturday. (46.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 167400 * MS_PER_SECOND);

  // 10:30 PM - 8:30 AM
  DoNotDisturbSchedule weekday_schedule_2 = {
    .from_hour = 22,
    .from_minute = 30,
    .to_hour = 8,
    .to_minute = 30,
  };

  // 12 AM - 10 AM
  DoNotDisturbSchedule weekend_schedule_2 = {
    .from_hour = 0,
    .from_minute = 0,
    .to_hour = 10,
    .to_minute = 0,
  };

  do_not_disturb_set_schedule(WeekdaySchedule, &weekday_schedule_2);
  do_not_disturb_set_schedule(WeekendSchedule, &weekend_schedule_2);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, true);

  rtc_set_time(s_friday_23_30); // In schedule
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 00:00 on Saturday. (0.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 1800 * MS_PER_SECOND);

  rtc_set_time(s_saturday_00_00);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 10:00 on Saturday. (10 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 36000 * MS_PER_SECOND);

  rtc_set_time(s_saturday_10_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 01:00 on Sunday. (13.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 48600 * MS_PER_SECOND);

  rtc_set_time(s_sunday_9_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);
  // Timer will go off at 10:00 on Sunday. (0.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 1800 * MS_PER_SECOND);

  rtc_set_time(s_sunday_10_00);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 01:00 on Sunday. (14 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 50400 * MS_PER_SECOND);

  rtc_set_time(s_sunday_23_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 00:00 on Monday. (0.5 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 1800 * MS_PER_SECOND);

  rtc_set_time(s_monday_10_30);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);
  // Timer will go off at 01:00 on Sunday. (12 hours)
  cl_assert_equal_i(stub_new_timer_timeout(get_dnd_timer_id()), 43200 * MS_PER_SECOND);
}

void test_do_not_disturb__toggle_manually_enabled(void) {
  bool active, enabled;

  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekdaySchedule) == false);
  cl_assert(do_not_disturb_is_schedule_enabled(WeekendSchedule) == false);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Quiet Time multi-schedule tests
///////////////////////////////////////////////////////////////////////////////////////////////////

void test_do_not_disturb__qt_create_delete_schedule(void) {
  QuietTimeScheduleConfig config = {
    .kind = QT_KIND_WEEKDAYS,
    .from_hour = 23,
    .from_minute = 0,
    .to_hour = 7,
    .to_minute = 0,
    .enabled = true,
  };
  memset(config.scheduled_days, 0, sizeof(config.scheduled_days));

  int idx = quiet_time_create_schedule(&config);
  cl_assert(idx >= 0);
  cl_assert(idx < MAX_QUIET_TIME_SCHEDULES);

  QuietTimeScheduleConfig retrieved;
  quiet_time_get_schedule(idx, &retrieved);
  cl_assert(retrieved.is_used == true);
  cl_assert(retrieved.kind == QT_KIND_WEEKDAYS);
  cl_assert(retrieved.from_hour == 23);
  cl_assert(retrieved.to_hour == 7);
  cl_assert(retrieved.enabled == true);

  quiet_time_delete_schedule(idx);
  quiet_time_get_schedule(idx, &retrieved);
  cl_assert(retrieved.is_used == false);
}

void test_do_not_disturb__qt_create_rejects_empty_custom(void) {
  QuietTimeScheduleConfig config = {
    .kind = QT_KIND_CUSTOM,
    .from_hour = 22,
    .from_minute = 0,
    .to_hour = 6,
    .to_minute = 0,
    .enabled = true,
  };
  memset(config.scheduled_days, 0, sizeof(config.scheduled_days));

  int idx = quiet_time_create_schedule(&config);
  cl_assert(idx == -1);
}

void test_do_not_disturb__qt_create_custom_with_days(void) {
  QuietTimeScheduleConfig config = {
    .kind = QT_KIND_CUSTOM,
    .from_hour = 22,
    .from_minute = 0,
    .to_hour = 6,
    .to_minute = 0,
    .enabled = true,
  };
  memset(config.scheduled_days, 0, sizeof(config.scheduled_days));
  config.scheduled_days[Monday] = true;
  config.scheduled_days[Wednesday] = true;
  config.scheduled_days[Friday] = true;

  int idx = quiet_time_create_schedule(&config);
  cl_assert(idx >= 0);

  QuietTimeScheduleConfig retrieved;
  quiet_time_get_schedule(idx, &retrieved);
  cl_assert(retrieved.kind == QT_KIND_CUSTOM);
  cl_assert(retrieved.scheduled_days[Monday] == true);
  cl_assert(retrieved.scheduled_days[Wednesday] == true);
  cl_assert(retrieved.scheduled_days[Friday] == true);
  cl_assert(retrieved.scheduled_days[Sunday] == false);
  cl_assert(retrieved.scheduled_days[Saturday] == false);

  quiet_time_delete_schedule(idx);
}

void test_do_not_disturb__qt_scheduled_days_derivation(void) {
  bool days[DAYS_PER_WEEK];

  QuietTimeScheduleConfig config_everyday = { .kind = QT_KIND_EVERYDAY };
  quiet_time_get_scheduled_days(&config_everyday, days);
  for (int i = 0; i < DAYS_PER_WEEK; i++) {
    cl_assert(days[i] == true);
  }

  QuietTimeScheduleConfig config_weekdays = { .kind = QT_KIND_WEEKDAYS };
  quiet_time_get_scheduled_days(&config_weekdays, days);
  cl_assert(days[Sunday] == false);
  cl_assert(days[Monday] == true);
  cl_assert(days[Saturday] == false);
  cl_assert(days[Friday] == true);

  QuietTimeScheduleConfig config_weekends = { .kind = QT_KIND_WEEKENDS };
  quiet_time_get_scheduled_days(&config_weekends, days);
  cl_assert(days[Sunday] == true);
  cl_assert(days[Monday] == false);
  cl_assert(days[Saturday] == true);
  cl_assert(days[Friday] == false);
}

void test_do_not_disturb__qt_display_strings(void) {
  const char *str;

  str = quiet_time_get_string_for_kind(QT_KIND_EVERYDAY);
  cl_assert_equal_s(str, "Every Day");

  str = quiet_time_get_string_for_kind(QT_KIND_WEEKDAYS);
  cl_assert_equal_s(str, "Weekdays");

  str = quiet_time_get_string_for_kind(QT_KIND_WEEKENDS);
  cl_assert_equal_s(str, "Weekends");

  str = quiet_time_get_string_for_kind(QT_KIND_CUSTOM);
  cl_assert_equal_s(str, "Custom");

  char buf[28];
  bool days[DAYS_PER_WEEK] = {false};
  days[Monday] = true;
  days[Wednesday] = true;
  days[Friday] = true;
  quiet_time_get_string_for_custom(days, buf, sizeof(buf));
  cl_assert_equal_s(buf, "Mon,Wed,Fri");

  bool single_day[DAYS_PER_WEEK] = {false};
  single_day[Tuesday] = true;
  quiet_time_get_string_for_custom(single_day, buf, sizeof(buf));
  cl_assert_equal_s(buf, "Tuesdays");
}

void test_do_not_disturb__qt_multi_schedule_active(void) {
  bool active;

  // Clean slate
  do_not_disturb_set_manually_enabled(false);
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    quiet_time_delete_schedule(i);
  }

  // Weekday schedule: 23:00 - 07:00
  QuietTimeScheduleConfig weekday_config = {
    .kind = QT_KIND_WEEKDAYS,
    .from_hour = 23,
    .from_minute = 0,
    .to_hour = 7,
    .to_minute = 0,
    .enabled = true,
  };
  memset(weekday_config.scheduled_days, 0, sizeof(weekday_config.scheduled_days));
  int weekday_idx = quiet_time_create_schedule(&weekday_config);
  cl_assert(weekday_idx >= 0);
  quiet_time_set_schedule_enabled(weekday_idx, true);

  // Set time to Thursday 8:30 - outside weekday schedule
  rtc_set_time(s_thursday_01_00);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false);

  // Set time to Thursday 01:30 - inside weekday schedule (23:00 - 07:00)
  rtc_set_time(1426125000); // Thursday 01:30
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Add weekend schedule: 01:00 - 09:00
  QuietTimeScheduleConfig weekend_config = {
    .kind = QT_KIND_WEEKENDS,
    .from_hour = 1,
    .from_minute = 0,
    .to_hour = 9,
    .to_minute = 0,
    .enabled = true,
  };
  memset(weekend_config.scheduled_days, 0, sizeof(weekend_config.scheduled_days));
  int weekend_idx = quiet_time_create_schedule(&weekend_config);
  cl_assert(weekend_idx >= 0);
  quiet_time_set_schedule_enabled(weekend_idx, true);

  // Still Thursday 01:30 - inside weekday schedule
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == true);

  // Disable weekday schedule
  quiet_time_set_schedule_enabled(weekday_idx, false);
  do_not_disturb_handle_clock_change();
  active = do_not_disturb_is_active();
  cl_assert(active == false); // Thursday, weekday disabled, weekend doesn't apply

  // Clean up
  quiet_time_delete_schedule(weekday_idx);
  quiet_time_delete_schedule(weekend_idx);
}

void test_do_not_disturb__qt_migration_from_legacy(void) {
  // Set up legacy weekday schedule
  DoNotDisturbSchedule legacy_schedule = {
    .from_hour = 22,
    .from_minute = 0,
    .to_hour = 6,
    .to_minute = 0,
  };
  do_not_disturb_set_schedule(WeekdaySchedule, &legacy_schedule);
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, true);

  // Verify the QT schedule was synced
  QuietTimeScheduleConfig qt_config;
  quiet_time_get_schedule(0, &qt_config);
  cl_assert(qt_config.kind == QT_KIND_WEEKDAYS);
  cl_assert(qt_config.from_hour == 22);
  cl_assert(qt_config.from_minute == 0);
  cl_assert(qt_config.to_hour == 6);
  cl_assert(qt_config.to_minute == 0);
  cl_assert(qt_config.enabled == true);

  // Set up legacy weekend schedule
  DoNotDisturbSchedule weekend_schedule = {
    .from_hour = 0,
    .from_minute = 0,
    .to_hour = 10,
    .to_minute = 0,
  };
  do_not_disturb_set_schedule(WeekendSchedule, &weekend_schedule);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, true);

  // Verify the QT schedule was synced
  quiet_time_get_schedule(1, &qt_config);
  cl_assert(qt_config.kind == QT_KIND_WEEKENDS);
  cl_assert(qt_config.from_hour == 0);
  cl_assert(qt_config.to_hour == 10);
  cl_assert(qt_config.enabled == true);

  // Clean up
  do_not_disturb_set_schedule_enabled(WeekdaySchedule, false);
  do_not_disturb_set_schedule_enabled(WeekendSchedule, false);
}

void test_do_not_disturb__qt_create_finds_unused_slot(void) {
  // Fill slots 0-1, leave slot 2 empty (is_used == false)
  QuietTimeScheduleConfig config0 = {
    .kind = QT_KIND_WEEKDAYS,
    .from_hour = 23,
    .from_minute = 0,
    .to_hour = 7,
    .to_minute = 0,
    .enabled = true,
  };
  memset(config0.scheduled_days, 0, sizeof(config0.scheduled_days));
  quiet_time_set_schedule(0, &config0);

  QuietTimeScheduleConfig config1 = {
    .kind = QT_KIND_WEEKENDS,
    .from_hour = 1,
    .from_minute = 0,
    .to_hour = 9,
    .to_minute = 0,
    .enabled = true,
  };
  memset(config1.scheduled_days, 0, sizeof(config1.scheduled_days));
  quiet_time_set_schedule(1, &config1);

  // Creating a new schedule should land in slot 2
  QuietTimeScheduleConfig new_config = {
    .kind = QT_KIND_EVERYDAY,
    .from_hour = 22,
    .from_minute = 0,
    .to_hour = 6,
    .to_minute = 0,
    .enabled = true,
  };
  int idx = quiet_time_create_schedule(&new_config);
  cl_assert(idx == 2);

  // Clean up
  quiet_time_delete_schedule(0);
  quiet_time_delete_schedule(1);
  quiet_time_delete_schedule(2);
}

void test_do_not_disturb__qt_create_returns_full_when_no_slots(void) {
  // Fill all slots
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    QuietTimeScheduleConfig config = {
      .kind = QT_KIND_EVERYDAY,
      .from_hour = 22,
      .from_minute = 0,
      .to_hour = 6,
      .to_minute = 0,
      .enabled = true,
    };
    quiet_time_set_schedule(i, &config);
  }

  // Should return -1 since all slots are full
  QuietTimeScheduleConfig overflow = {
    .kind = QT_KIND_EVERYDAY,
    .from_hour = 10,
    .from_minute = 0,
    .to_hour = 14,
    .to_minute = 0,
    .enabled = true,
  };
  int idx = quiet_time_create_schedule(&overflow);
  cl_assert(idx == -1);

  // Clean up
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    quiet_time_delete_schedule(i);
  }
}

//! Phone-originated receive-path tests.

static void prv_simulate_phone_write(const char *key, const void *value, size_t value_len) {
  // Mirror the real blob DB write path: write to settings_file, then
  // dispatch the receive event.
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "notifpref", 1024));
  cl_must_pass(settings_file_set(&file, key, strlen(key), value, value_len));
  settings_file_close(&file);

  PebbleBlobDBEvent event = {
    .db_id = BlobDBIdSettings,
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = (int)strlen(key),
  };
  alerts_preferences_handle_blob_db_event(&event);
}

void test_do_not_disturb__phone_manually_enabled_recomputes_active_state(void) {
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 0);
  cl_assert_equal_i(s_num_pref_change_events_put, 0);

  // Phone toggles Manual ON.
  const bool enabled = true;
  prv_simulate_phone_write("dndManuallyEnabled", &enabled, sizeof(enabled));

  cl_assert(do_not_disturb_is_manually_enabled() == true);
  // Active state is recomputed and a DND event posted.
  cl_assert(do_not_disturb_is_active() == true);
  cl_assert_equal_i(s_num_dnd_events_put, 1);
  cl_assert_equal_i(s_num_pref_change_events_put, 1);
  prv_assert_manually_dnd_setting_val(true);

  // Phone toggles Manual OFF.
  const bool disabled = false;
  prv_simulate_phone_write("dndManuallyEnabled", &disabled, sizeof(disabled));

  cl_assert(do_not_disturb_is_manually_enabled() == false);
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 2);
  cl_assert_equal_i(s_num_pref_change_events_put, 2);
  prv_assert_manually_dnd_setting_val(false);
}

void test_do_not_disturb__phone_smart_enabled_recomputes_active_state(void) {
  calendar_init();
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 0);
  cl_assert_equal_i(s_num_pref_change_events_put, 0);

  // Phone toggles Smart ON; no calendar event so no DND event yet.
  const bool enabled = true;
  prv_simulate_phone_write("dndSmartEnabled", &enabled, sizeof(enabled));

  cl_assert(do_not_disturb_is_smart_dnd_enabled() == true);
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 0);
  cl_assert_equal_i(s_num_pref_change_events_put, 1);

  // Calendar event starts; refresh posts a DND event.
  s_event_ongoing = true;
  cl_assert(do_not_disturb_is_active() == true);
  do_not_disturb_refresh_active_state();
  cl_assert_equal_i(s_num_dnd_events_put, 1);

  s_event_ongoing = false;
  do_not_disturb_refresh_active_state();
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 2);

  // Phone toggles Smart OFF.
  const bool disabled = false;
  prv_simulate_phone_write("dndSmartEnabled", &disabled, sizeof(disabled));
  cl_assert(do_not_disturb_is_smart_dnd_enabled() == false);
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 2);
  cl_assert_equal_i(s_num_pref_change_events_put, 2);
}

//! Refresh is a no-op when the active state didn't change.
void test_do_not_disturb__refresh_active_state_noop_when_unchanged(void) {
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 0);

  do_not_disturb_refresh_active_state();
  cl_assert(do_not_disturb_is_active() == false);
  cl_assert_equal_i(s_num_dnd_events_put, 0);
}

