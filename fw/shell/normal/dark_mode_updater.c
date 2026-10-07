/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_updater.h"

#include <pbl/services/light.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/system_task.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>
#include <system/passert.h>

#define AMBIENT_REFRESH_INTERVAL_MS (10 * 1000)

static TimerID s_timer = TIMER_INVALID_ID;

static void prv_timer_callback(void *data);

static void prv_refresh_callback(void *data) {
  if (shell_prefs_get_dark_mode() != DarkModeAmbient) {
    return;
  }
  // The backlight lights up the sensor, which would read as "light"; keep the last sample
  if (!light_is_on()) {
    // Runs on the system task: sampling the ambient light sensor can block
    system_theme_refresh_ambient();
  }
  // Don't override an immediate refresh requested by a prefs change while we were running
  new_timer_start(s_timer, AMBIENT_REFRESH_INTERVAL_MS, prv_timer_callback, NULL,
                  TIMER_START_FLAG_FAIL_IF_SCHEDULED);
}

static void prv_timer_callback(void *data) {
  system_task_add_callback(prv_refresh_callback, NULL);
}

void dark_mode_updater_prefs_changed(void) {
  if (s_timer == TIMER_INVALID_ID) {
    return;
  }
  if (shell_prefs_get_dark_mode() == DarkModeAmbient) {
    new_timer_start(s_timer, 1 /* ms */, prv_timer_callback, NULL, 0 /* flags */);
  } else {
    new_timer_stop(s_timer);
  }
}

void dark_mode_updater_init(void) {
  s_timer = new_timer_create();
  PBL_ASSERTN(s_timer != TIMER_INVALID_ID);
  dark_mode_updater_prefs_changed();
}
