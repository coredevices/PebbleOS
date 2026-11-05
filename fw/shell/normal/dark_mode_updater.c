/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_updater.h"

#include <pbl/services/clock.h>
#include <pbl/services/regular_timer.h>

#include <kernel/events.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

#define MINUTES_PER_DAY (24 * 60)

//! Last state announced with PEBBLE_DARK_MODE_CHANGE_EVENT
static bool s_is_dark;

static void prv_minute_callback(void *data);

static RegularTimerInfo s_minute_timer = {
  .cb = prv_minute_callback,
};

bool dark_mode_schedule_is_active(const DarkModeSchedule *schedule, int minute_of_day) {
  const int start = schedule->from_hour * 60 + schedule->from_minute;
  const int end = schedule->to_hour * 60 + schedule->to_minute;
  // Measure from the start so a window that wraps past midnight needs no special case
  const int since_start = (minute_of_day - start + MINUTES_PER_DAY) % MINUTES_PER_DAY;
  const int length = (end - start + MINUTES_PER_DAY) % MINUTES_PER_DAY;
  return since_start < length;
}

static bool prv_is_dark(void) {
  switch (shell_prefs_get_dark_mode()) {
    case DarkModeOn:
      return true;
    case DarkModeScheduled: {
      DarkModeSchedule schedule;
      shell_prefs_get_dark_mode_schedule(&schedule);
      struct pbl_tm now;
      clock_get_time_tm(&now);
      return dark_mode_schedule_is_active(&schedule, now.tm_hour * 60 + now.tm_min);
    }
    default:
      return false;
  }
}

static void prv_update(void) {
  const bool is_dark = prv_is_dark();
  if (is_dark == s_is_dark) {
    return;
  }
  s_is_dark = is_dark;
  system_theme_set_dark_mode(is_dark);
  PebbleEvent event = {
    .type = PEBBLE_DARK_MODE_CHANGE_EVENT,
  };
  event_put(&event);
}

static void prv_minute_callback(void *data) {
  prv_update();
}

void dark_mode_updater_prefs_changed(void) {
  prv_update();
  // The schedule has minute resolution, so follow the wall-clock minute tick
  if (shell_prefs_get_dark_mode() == DarkModeScheduled) {
    regular_timer_add_minutes_callback(&s_minute_timer);
  } else if (regular_timer_is_scheduled(&s_minute_timer)) {
    regular_timer_remove_callback(&s_minute_timer);
  }
}

void dark_mode_updater_init(void) {
  s_is_dark = prv_is_dark();
  system_theme_set_dark_mode(s_is_dark);
  dark_mode_updater_prefs_changed();
}
