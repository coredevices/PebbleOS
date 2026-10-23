/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_updater.h"

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/assert.h>
#include <pbl/services/light.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/system_task.h>
#include <kernel/events.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

#define AMBIENT_REFRESH_INTERVAL_MS (10 * 1000)
//! The schedule has minute resolution; land just after the minute rolls over
#define SCHEDULE_REFRESH_SLACK_MS 100

static TimerID s_timer = TIMER_INVALID_ID;

//! Last state announced with PEBBLE_DARK_MODE_CHANGE_EVENT. Only touched from the system task
//! once the timer is running.
static bool s_is_dark;

static void prv_timer_callback(void *data);

//! @return How long to wait before checking again, or 0 if nothing can flip the mode by itself
static uint32_t prv_get_refresh_interval_ms(void) {
  switch (shell_prefs_get_dark_mode()) {
    case DarkModeAmbient:
      return AMBIENT_REFRESH_INTERVAL_MS;
    case DarkModeScheduled: {
      struct pbl_tm time_now;
      rtc_get_time_tm(&time_now);
      return (60 - time_now.tm_sec) * 1000 + SCHEDULE_REFRESH_SLACK_MS;
    }
    default:
      return 0;
  }
}

static void prv_notify_if_changed(void) {
  const bool is_dark = system_theme_is_dark_mode();
  if (is_dark == s_is_dark) {
    return;
  }
  s_is_dark = is_dark;
  PebbleEvent event = {
    .type = PEBBLE_DARK_MODE_CHANGE_EVENT,
  };
  event_put(&event);
}

static void prv_refresh_callback(void *data) {
  // The backlight lights up the sensor, which would read as "light"; keep the last sample
  if (shell_prefs_get_dark_mode() == DarkModeAmbient && !light_is_on()) {
    // Runs on the system task: sampling the ambient light sensor can block
    system_theme_refresh_ambient();
  }
  prv_notify_if_changed();

  const uint32_t interval_ms = prv_get_refresh_interval_ms();
  if (interval_ms != 0) {
    // Don't override an immediate refresh requested by a prefs change while we were running
    new_timer_start(s_timer, interval_ms, prv_timer_callback, NULL,
                    TIMER_START_FLAG_FAIL_IF_SCHEDULED);
  }
}

static void prv_timer_callback(void *data) {
  system_task_add_callback(prv_refresh_callback, NULL);
}

void dark_mode_updater_prefs_changed(void) {
  if (s_timer == TIMER_INVALID_ID) {
    return;
  }
  // Re-evaluate right away: the pref change itself can flip the mode, including switching it off
  new_timer_start(s_timer, 1 /* ms */, prv_timer_callback, NULL, 0 /* flags */);
}

void dark_mode_updater_init(void) {
  s_timer = new_timer_create();
  PBL_ASSERTN(s_timer != TIMER_INVALID_ID);
  s_is_dark = system_theme_is_dark_mode();
  dark_mode_updater_prefs_changed();
}
