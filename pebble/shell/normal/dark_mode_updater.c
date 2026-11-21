/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_updater.h"

#include <pbl/kernel/work.h>
#include <pbl/services/clock.h>

#include <kernel/events.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

#define MINUTES_PER_DAY (24 * 60)

//! Last state announced with PEBBLE_DARK_MODE_CHANGE_EVENT
static bool s_is_dark;

static struct pbl_wallclock_work s_minute_timer;

bool dark_mode_schedule_is_active(const DarkModeSchedule *schedule, int minute_of_day) {
  const int start = schedule->from_hour * 60 + schedule->from_minute;
  const int end = schedule->to_hour * 60 + schedule->to_minute;
  // Measure from the start so a window that wraps past midnight needs no special case
  const int since_start = (minute_of_day - start + MINUTES_PER_DAY) % MINUTES_PER_DAY;
  const int length = (end - start + MINUTES_PER_DAY) % MINUTES_PER_DAY;
  return since_start < length;
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static bool prv_is_dark_after_sunset(int minute_of_day) {
  // Same sunrise/sunset the night backlight color uses, kept current by the phone
  const uint16_t sunrise = backlight_get_sunrise_minute();
  const uint16_t sunset = backlight_get_sunset_minute();
  if (sunrise >= MINUTES_PER_DAY || sunset >= MINUTES_PER_DAY) {
    return false;
  }
  const DarkModeSchedule schedule = {
    .from_hour = sunset / 60,
    .from_minute = sunset % 60,
    .to_hour = sunrise / 60,
    .to_minute = sunrise % 60,
  };
  return dark_mode_schedule_is_active(&schedule, minute_of_day);
}
#endif

static bool prv_follows_clock(DarkMode mode) {
  return mode == DarkModeScheduled || mode == DarkModeSunset;
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
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
    case DarkModeSunset: {
      struct pbl_tm now;
      clock_get_time_tm(&now);
      return prv_is_dark_after_sunset(now.tm_hour * 60 + now.tm_min);
    }
#endif
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

static void prv_minute_callback(struct pbl_work *work) {
  prv_update();
}

void dark_mode_updater_prefs_changed(void) {
  prv_update();
  // The schedule has minute resolution, so follow the wall clock's minute
  if (prv_follows_clock(shell_prefs_get_dark_mode())) {
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_minute_timer, 60, 1);
  } else {
    pbl_wallclock_work_stop(&s_minute_timer);
  }
}

void dark_mode_updater_init(void) {
  pbl_wallclock_work_init(&s_minute_timer, prv_minute_callback);
  s_is_dark = prv_is_dark();
  system_theme_set_dark_mode(s_is_dark);
  dark_mode_updater_prefs_changed();
}
