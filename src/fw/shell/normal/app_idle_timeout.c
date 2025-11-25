/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_idle_timeout.h"

#include "kernel/event_loop.h"
#include "os/tick.h"
#include "services/common/new_timer/new_timer.h"
#include "shell/normal/watchface.h"
#include "shell/shell.h"
#include "system/logging.h"
#include "system/passert.h"


static const int WATCHFACE_TIMEOUT_MS = 30000;

TimerID s_timer;
bool s_app_paused = false;
bool s_app_started = false;

#ifndef NO_WATCH_TIMEOUT
static void prv_kernel_callback_watchface_launch(void* data) {
  watchface_launch_default(shell_get_watchface_compositor_animation(true /* watchface_is_dest */));
}

static void prv_timeout_expired(void *cb_data) {
  PBL_LOG(LOG_LEVEL_DEBUG, "App idle timeout hit! launching watchface");
  launcher_task_add_callback(prv_kernel_callback_watchface_launch, NULL);
}

static void prv_start_timer(bool create) {
  if (create) {
    s_timer = new_timer_create();
  }

  if (s_timer != TIMER_INVALID_ID && !s_app_paused && s_app_started) {
    bool success = new_timer_start(s_timer, WATCHFACE_TIMEOUT_MS, prv_timeout_expired,
        NULL, 0 /* flags */);
    PBL_ASSERTN(success);
  }
}
#endif

void app_idle_timeout_start(void) {
  PBL_ASSERTN(s_timer == TIMER_INVALID_ID);

  s_app_started = true;
#ifndef NO_WATCH_TIMEOUT
  prv_start_timer(true /* create a timer */);
#endif
}

void app_idle_timeout_stop(void) {
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
#ifndef NO_WATCH_TIMEOUT
  prv_start_timer(false /* do not create a timer */);
#endif
}

void app_idle_timeout_refresh(void) {
#ifndef NO_WATCH_TIMEOUT
  prv_start_timer(false /* do not create a timer */);
#endif
}
