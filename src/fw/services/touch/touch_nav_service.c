/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/touch/touch_nav_service.h"

#ifdef CONFIG_TOUCH

#include "applib/ui/recognizer/touch_nav.h"
#include "kernel/event_loop.h"
#include "kernel/pebble_tasks.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/touch/touch.h"
#include "process_management/process_manager.h"
#include "process_state/app_state/app_state.h"

// --- KernelMain-bound effects -----------------------------------------------------------------

static void prv_kernel_subscribe_cb(void *unused) {
  modal_touch_nav_subscribe();
  // Take the permanent sensor hold here on KernelMain so the sensor stays powered while nav is on.
  touch_set_system_hold(true);
}

static void prv_kernel_unsubscribe_cb(void *unused) {
  modal_touch_nav_unsubscribe();
}

static void prv_kernel_release_hold_cb(void *unused) {
  // Release the permanent sensor hold on KernelMain. Queued AFTER prv_kernel_unsubscribe_cb so the
  // hold outlives the kernel unsubscribe (disable step 5 runs after step 3, not before it).
  touch_set_system_hold(false);
}

// --- App-task-bound effects -------------------------------------------------------------------

static void prv_app_subscribe_cb(void *unused) {
  app_touch_nav_subscribe();
}

static void prv_app_unsubscribe_cb(void *unused) {
  app_touch_nav_unsubscribe();
}

// --- Concrete transaction ops ------------------------------------------------------------------

static void prv_persist(void *ctx, bool enable) {
  // The pref file write is done by the shell pref system before we run; flip the runtime gate.
  touch_set_nav_enabled(enable);
}

static void prv_kernel_subscribe(void *ctx) {
  launcher_task_add_callback(prv_kernel_subscribe_cb, NULL);
}

static void prv_take_system_hold(void *ctx) {
  // Folded into the KernelMain callback above; nothing to do synchronously.
}

static void prv_synthesize_liftoff(void *ctx) {
  // Narrow contract: unwinds the backlight hold and driver state only (does not reach managers).
  touch_release_active();
}

static void prv_kernel_cancel_reset_unsub(void *ctx) {
  launcher_task_add_callback(prv_kernel_unsubscribe_cb, NULL);
}

static void prv_app_unsubscribe(void *ctx) {
  process_manager_send_callback_event_to_process(PebbleTask_App, prv_app_unsubscribe_cb, NULL);
}

static void prv_release_system_hold(void *ctx) {
  // Defer to KernelMain instead of releasing synchronously on the calling task: steps 3 (kernel
  // cancel_and_reset + unsubscribe) are already queued on KernelMain, so scheduling the release
  // after them makes the actual async order 2->3->5 rather than releasing the hold before 3-4 run.
  launcher_task_add_callback(prv_kernel_release_hold_cb, NULL);
}

static const TouchNavTxnOps s_txn_ops = {
  .persist = prv_persist,
  .kernel_subscribe = prv_kernel_subscribe,
  .take_system_hold = prv_take_system_hold,
  .synthesize_liftoff = prv_synthesize_liftoff,
  .kernel_cancel_reset_unsub = prv_kernel_cancel_reset_unsub,
  .app_unsubscribe = prv_app_unsubscribe,
  .release_system_hold = prv_release_system_hold,
};

void touch_nav_set_enabled(bool enable) {
  touch_nav_transaction_apply(&s_txn_ops, enable);
  if (enable) {
    // Also pick up any already-running app so nav starts working without a relaunch.
    process_manager_send_callback_event_to_process(PebbleTask_App, prv_app_subscribe_cb, NULL);
  }
}

#endif  // CONFIG_TOUCH
