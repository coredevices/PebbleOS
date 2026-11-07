/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/kernel/work.h>
#include <pbl/util/math.h>

#include <applib/ui/animation_private.h>
#include <kernel/events.h>
#include <kernel/kernel_applib_state.h>
#include <kernel/pebble_tasks.h>
#include <process_management/process_manager.h>
#include <process_state/app_state/app_state.h>

static void prv_timer_callback(struct pbl_work *work);

// The timer used for each task that we support
static PBL_DELAYED_WORK_DEFINE(s_kernel_main_timer, prv_timer_callback);
static PBL_DELAYED_WORK_DEFINE(s_app_timer, prv_timer_callback);

static bool s_kernel_main_event_pending;
static bool s_app_event_pending;

// ------------------------------------------------------------------------------------------
void animation_service_cleanup(PebbleTask task) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);

  if (task == PebbleTask_KernelMain) {
    pbl_delayed_work_cancel(&s_kernel_main_timer);
    s_kernel_main_event_pending = false;
  } else if (task == PebbleTask_App) {
    pbl_delayed_work_cancel(&s_app_timer);
    s_app_event_pending = false;
  }
}

// ------------------------------------------------------------------------------------------
static void prv_timer_callback(struct pbl_work *work) {
  PebbleEvent e = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {.callback = animation_private_timer_callback}
  };

  if (work == &s_kernel_main_timer.work) {
    if (!s_kernel_main_event_pending) {
      s_kernel_main_event_pending = true;
      e.callback.data = (void *)kernel_applib_get_animation_state();
      event_put(&e);
    }
  } else if (!s_app_event_pending) {
    e.callback.data = (void *)app_state_get_animation_state();
    s_app_event_pending = process_manager_send_event_to_process(PebbleTask_App, &e);
  }
}

// ------------------------------------------------------------------------------------------
PBL_SYSCALL(void, animation_service_timer_event_received) {
  PebbleTask task = pebble_task_get_current();

  if (task == PebbleTask_KernelMain) {
    s_kernel_main_event_pending = false;
  } else if (task == PebbleTask_App) {
    s_app_event_pending = false;
  } else if (from_user) {
    pbl_syscall_fail();
  }
}

// ------------------------------------------------------------------------------------------
PBL_SYSCALL(void, animation_service_timer_schedule, (VAL, uint32_t, ms)) {
  PebbleTask task = pebble_task_get_current();
  struct pbl_delayed_work *timer;

  if (task == PebbleTask_KernelMain) {
    timer = &s_kernel_main_timer;
  } else if (task == PebbleTask_App) {
    timer = &s_app_timer;
  } else {
    if (from_user) {
      pbl_syscall_fail();
    }
    return;
  }

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, timer, PBL_MSEC(ms));
}

// ---------------------------------------------------------------------------
// Used for unit tests only
struct pbl_delayed_work *animation_service_test_get_timer(void) {
  return &s_kernel_main_timer;
}
