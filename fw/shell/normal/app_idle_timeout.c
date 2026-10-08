/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_idle_timeout.h"

#include <kernel/event_loop.h>
#include <pbl/services/new_timer/new_timer.h>
#include <shell/normal/watchface.h>
#include <shell/system_shell.h>
#include <pbl/logging/logging.h>
#include <system/passert.h>

TimerID s_timer;
uint32_t s_timeout_ms;
// Bumped when an app stops using the timeout, so stale duration changes queued by it are dropped
static uint32_t s_generation;
static uint32_t s_pending_timeout_ms;
bool s_app_paused = false;
bool s_app_started = false;
// Tracks the physical finger, independent of the app lifecycle: a finger on the screen halts the
// timeout even across focus pause/resume, and liftoff restarts it only if nothing else pauses it.
bool s_touch_held = false;

#ifndef CONFIG_NO_WATCH_TIMEOUT
static void prv_kernel_callback_watchface_launch(void *data) {
  watchface_launch_default(shell_get_watchface_compositor_animation(true /* watchface_is_dest */));
}

static void prv_timeout_expired(void *cb_data) {
  PBL_LOG_DBG("App idle timeout hit! launching watchface");
  launcher_task_add_callback(prv_kernel_callback_watchface_launch, NULL);
}

static void prv_start_timer(bool create) {
  if (create) {
    s_timer = new_timer_create();
  }

  if (s_timer != TIMER_INVALID_ID && !s_app_paused && !s_touch_held && s_app_started) {
    bool success = new_timer_start(s_timer, s_timeout_ms, prv_timeout_expired, NULL, 0 /* flags */);
    PBL_ASSERTN(success);
  }
}
#endif

void app_idle_timeout_start(uint32_t timeout_ms) {
  PBL_ASSERTN(s_timer == TIMER_INVALID_ID);

  s_timeout_ms = timeout_ms;
  s_app_started = true;
#ifndef CONFIG_NO_WATCH_TIMEOUT
  prv_start_timer(true /* create a timer */);
#endif
}

static void prv_kernel_callback_set_duration(void *data) {
  if ((uint32_t)(uintptr_t)data != s_generation) {
    return;
  }
  s_timeout_ms = s_pending_timeout_ms;
  app_idle_timeout_refresh();
}

void app_idle_timeout_set_duration(uint32_t timeout_ms) {
  // Timer state is owned by KernelMain; hop there so pause/touch can't race the restart
  s_pending_timeout_ms = timeout_ms;
  launcher_task_add_callback(prv_kernel_callback_set_duration, (void *)(uintptr_t)s_generation);
}

void app_idle_timeout_stop(void) {
  s_generation++;
  if (s_timer != TIMER_INVALID_ID) {
    new_timer_delete(s_timer);
    s_timer = TIMER_INVALID_ID;
    s_app_started = false;
  }
}

void app_idle_timeout_pause(void) {
  if (s_timer != TIMER_INVALID_ID) {
    new_timer_stop(s_timer);
  }
  s_app_paused = true;
}

void app_idle_timeout_resume(void) {
  s_app_paused = false;
#ifndef CONFIG_NO_WATCH_TIMEOUT
  prv_start_timer(false /* do not create a timer */);
#endif
}

void app_idle_timeout_refresh(void) {
#ifndef CONFIG_NO_WATCH_TIMEOUT
  prv_start_timer(false /* do not create a timer */);
#endif
}

void app_idle_timeout_touch_down(void) {
  s_touch_held = true;
  if (s_timer != TIMER_INVALID_ID) {
    new_timer_stop(s_timer);
  }
}

void app_idle_timeout_touch_up(void) {
  s_touch_held = false;
#ifndef CONFIG_NO_WATCH_TIMEOUT
  prv_start_timer(false /* do not create a timer */);
#endif
}
