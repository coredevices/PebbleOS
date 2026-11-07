/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>

#include <applib/app.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/window.h>
#include <profile_mutexes.h>
#include <syscall/syscall.h>

static Window *window;
static PBL_MUTEX_DEFINE(s_mutex);
static PBL_MUTEX_DEFINE(s_mutex2);

static void callback(struct pbl_work *work) {
  PBL_LOG_DBG("Locking mutex 2 (work)");
  pbl_mutex_lock(&s_mutex2, PBL_FOREVER);
  PBL_LOG_DBG("Locking mutex 1 (work)");
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
}

static PBL_DELAYED_WORK_DEFINE(s_work, callback);

static void deadlock(void) {
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_work, PBL_MSEC(10));

  PBL_LOG_DBG("Locking mutex 1");
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  sys_psleep(20);
  PBL_LOG_DBG("Locking mutex 2");
  pbl_mutex_lock(&s_mutex2, PBL_FOREVER);
}

static void s_main(void) {
  window = window_create();
  app_window_stack_push(window, true /* Animated */);

  deadlock();

  app_event_loop();
}

const PebbleProcessMd *deadlock_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {.common.main_func = &s_main, .name = "Deadlock"};
  return (const PebbleProcessMd *)&s_app_info;
}
