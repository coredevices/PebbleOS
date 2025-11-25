/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "profile_mutexes_app.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"

#include "system/logging.h"
#include "os/mutex.h"
#include "system/profiler.h"

#include "kernel/util/sleep.h"
#include "services/common/new_timer/new_timer.h"

static Window *window;
static PebbleMutex *s_mutex;
static PebbleMutex *s_mutex2;

static void callback(void *data) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Locking mutex 2 (new timer)");
  mutex_lock(s_mutex2);
  PBL_LOG(LOG_LEVEL_DEBUG, "Locking mutex 1 (new timer)");
  mutex_lock(s_mutex);
}

static void deadlock(void) {
  s_mutex = mutex_create();
  s_mutex2 = mutex_create();
  TimerID timer = new_timer_create();
  new_timer_start(timer, 10, callback, NULL, 0);

  PBL_LOG(LOG_LEVEL_DEBUG, "Locking mutex 1");
  mutex_lock(s_mutex);
  psleep(20);
  PBL_LOG(LOG_LEVEL_DEBUG, "Locking mutex 2");
  mutex_lock(s_mutex2);
}

static void s_main(void) {
  window = window_create();
  app_window_stack_push(window, true /* Animated */);

  deadlock();

  app_event_loop();
}

const PebbleProcessMd* deadlock_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Deadlock"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
