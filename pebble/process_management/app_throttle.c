/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_throttle.h"

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/task_wdt/task_wdt.h>

#include <kernel/pebble_tasks.h>

#define APP_THROTTLE_TIME_MS 300

static void prv_throttle_end(struct pbl_work *work) {
  struct pbl_thread *app = pebble_task_get_thread(PebbleTask_App);
  if (app) {
    pbl_thread_prio_set(app, CONFIG_APP_THREAD_PRIO);
  }
  PBL_LOG_DBG("Ending App Throttling");
}

static PBL_DELAYED_WORK_DEFINE(s_throttle_end, prv_throttle_end);

static void prv_throttle_start(void) {
  static char s_last_throttled_app[PBL_THREAD_NAME_LEN];
  struct pbl_thread *app = pebble_task_get_thread(PebbleTask_App);
  if (!app) {
    return;
  }

  const char *name = pbl_thread_name(app);
  if (strcmp(s_last_throttled_app, name) != 0) {
    strcpy(s_last_throttled_app, name);
    PBL_LOG_WRN("Starting App Throttling for %s", name);
  } else {
    PBL_LOG_DBG("Starting App Throttling for %s", name);
  }

  pbl_thread_prio_set(app, PBL_PRIO_IDLE);
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_throttle_end, PBL_MSEC(APP_THROTTLE_TIME_MS));
}

//! The queue is starved when it is ready to run but does not get the CPU, or blocked in a function
//! on a lock the worker cannot release because the app hogs the CPU. Parking the app briefly
//! resolves both.
static void *prv_expired(int channel_id, void *user_data) {
  const bool ready = pbl_thread_state(pbl_workq_get_thread(&pbl_sys_workq)) == PBL_THREAD_READY;

  if (ready || pbl_sys_work_current_fn() != nullptr) {
    prv_throttle_start();
  }
  return pbl_sys_work_current_fn();
}

void app_throttle_init(void) {
  PBL_ASSERTN(pbl_task_wdt_add_workq(&pbl_sys_workq, CONFIG_TASK_WDT_TIMEOUT_MS, prv_expired,
                                     nullptr) >= 0);
}
