/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/time.h>

#include <clar.h>
#include <kernel/events.h>
#include <shell/normal/dark_mode_updater.h>
#include <shell/prefs.h>

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_rtc.h>
#include <fake_work.h>

static DarkMode s_mode;
static DarkModeSchedule s_schedule;

DarkMode shell_prefs_get_dark_mode(void) {
  return s_mode;
}

void shell_prefs_get_dark_mode_schedule(DarkModeSchedule *schedule_out) {
  *schedule_out = s_schedule;
}

static bool s_theme_dark;

void system_theme_set_dark_mode(bool dark_mode) {
  s_theme_dark = dark_mode;
}

static int s_num_events;

void event_put(PebbleEvent *event) {
  cl_assert_equal_i(event->type, PEBBLE_DARK_MODE_CHANGE_EVENT);
  s_num_events++;
}

// Helpers
////////////////////////////////////////////////////////////////

static void prv_set_time(int hour, int minute) {
  pbl_realtime_set((hour * 60 + minute) * 60);
}

static bool prv_follows_minutes(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fake_wallclock_started); i++) {
    const struct pbl_wallclock_work *w = s_fake_wallclock_started[i];
    if (w != nullptr && w->unit_ms == 60000U && w->every == 1U) {
      return true;
    }
  }
  return false;
}

static void prv_fire_minute(int hour, int minute) {
  prv_set_time(hour, minute);
  fake_wallclock_work_advance(60, 1);
}

static bool prv_is_active(int from_hour, int from_minute, int to_hour, int to_minute, int hour,
                          int minute) {
  const DarkModeSchedule schedule = {
    .from_hour = from_hour,
    .from_minute = from_minute,
    .to_hour = to_hour,
    .to_minute = to_minute,
  };
  return dark_mode_schedule_is_active(&schedule, hour * 60 + minute);
}

// Setup
////////////////////////////////////////////////////////////////

void test_dark_mode_updater__initialize(void) {
  fake_rtc_init(0, 0);
  s_mode = DarkModeOff;
  s_schedule = (DarkModeSchedule){.from_hour = 19, .from_minute = 0, .to_hour = 7};
  s_theme_dark = false;
  s_num_events = 0;
}

void test_dark_mode_updater__cleanup(void) {
  fake_wallclock_work_cleanup();
}

// Tests
////////////////////////////////////////////////////////////////

void test_dark_mode_updater__schedule_within_day(void) {
  cl_assert(!prv_is_active(9, 30, 17, 0, 9, 29));
  cl_assert(prv_is_active(9, 30, 17, 0, 9, 30));
  cl_assert(prv_is_active(9, 30, 17, 0, 16, 59));
  cl_assert(!prv_is_active(9, 30, 17, 0, 17, 0));
  cl_assert(!prv_is_active(9, 30, 17, 0, 23, 59));
}

void test_dark_mode_updater__schedule_wraps_past_midnight(void) {
  cl_assert(!prv_is_active(19, 0, 7, 0, 18, 59));
  cl_assert(prv_is_active(19, 0, 7, 0, 19, 0));
  cl_assert(prv_is_active(19, 0, 7, 0, 23, 59));
  cl_assert(prv_is_active(19, 0, 7, 0, 0, 0));
  cl_assert(prv_is_active(19, 0, 7, 0, 6, 59));
  cl_assert(!prv_is_active(19, 0, 7, 0, 7, 0));
  cl_assert(!prv_is_active(19, 0, 7, 0, 12, 0));
}

void test_dark_mode_updater__schedule_ends_at_midnight(void) {
  cl_assert(prv_is_active(22, 0, 0, 0, 23, 59));
  cl_assert(!prv_is_active(22, 0, 0, 0, 0, 0));
  cl_assert(prv_is_active(0, 0, 6, 0, 0, 0));
  cl_assert(!prv_is_active(0, 0, 6, 0, 23, 59));
}

void test_dark_mode_updater__schedule_start_equals_end_is_empty(void) {
  for (int minute = 0; minute < 24 * 60; minute++) {
    cl_assert(!prv_is_active(8, 15, 8, 15, minute / 60, minute % 60));
  }
}

void test_dark_mode_updater__off_and_on(void) {
  dark_mode_updater_init();
  cl_assert(!s_theme_dark);
  cl_assert(!prv_follows_minutes());

  s_mode = DarkModeOn;
  dark_mode_updater_prefs_changed();
  cl_assert(s_theme_dark);
  cl_assert_equal_i(s_num_events, 1);
  // Nothing to follow outside of Scheduled mode
  cl_assert(!prv_follows_minutes());

  // No event when the effective mode stays the same
  dark_mode_updater_prefs_changed();
  cl_assert_equal_i(s_num_events, 1);

  s_mode = DarkModeOff;
  dark_mode_updater_prefs_changed();
  cl_assert(!s_theme_dark);
  cl_assert_equal_i(s_num_events, 2);
}

void test_dark_mode_updater__init_does_not_announce(void) {
  s_mode = DarkModeOn;
  dark_mode_updater_init();
  cl_assert(s_theme_dark);
  cl_assert_equal_i(s_num_events, 0);
}

void test_dark_mode_updater__schedule_follows_minute_tick(void) {
  prv_set_time(18, 58);
  s_mode = DarkModeScheduled;
  dark_mode_updater_init();
  cl_assert(!s_theme_dark);
  cl_assert(prv_follows_minutes());

  prv_fire_minute(18, 59);
  cl_assert(!s_theme_dark);
  cl_assert_equal_i(s_num_events, 0);

  prv_fire_minute(19, 0);
  cl_assert(s_theme_dark);
  cl_assert_equal_i(s_num_events, 1);

  prv_fire_minute(19, 1);
  cl_assert_equal_i(s_num_events, 1);

  prv_fire_minute(7, 0);
  cl_assert(!s_theme_dark);
  cl_assert_equal_i(s_num_events, 2);

  // Leaving Scheduled mode stops following the clock
  s_mode = DarkModeOff;
  dark_mode_updater_prefs_changed();
  cl_assert(!prv_follows_minutes());
  cl_assert_equal_i(s_num_events, 2);
}

void test_dark_mode_updater__schedule_change_applies_right_away(void) {
  prv_set_time(12, 0);
  s_mode = DarkModeScheduled;
  dark_mode_updater_init();
  cl_assert(!s_theme_dark);

  s_schedule = (DarkModeSchedule){.from_hour = 11, .from_minute = 30, .to_hour = 13};
  dark_mode_updater_prefs_changed();
  cl_assert(s_theme_dark);
  cl_assert_equal_i(s_num_events, 1);
}
