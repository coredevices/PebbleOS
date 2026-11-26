/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/thread.h>
#include <pbl/task_wdt/task_wdt.h>

#include <system/die.h>
#include <system/reboot_reason.h>
#include <system/task_wdt.h>

static bool prv_reboot_reason_is_ours(void) {
  RebootReason current;
  reboot_reason_get(&current);
  return current.code == RebootReasonCode_Unknown || current.code == RebootReasonCode_Watchdog;
}

void pbl_task_wdt_stalled(const struct pbl_task_wdt_stall *stall) {
  RebootReason reason = {
    .code = RebootReasonCode_Watchdog,
    .data8 = {stall->fed_mask, stall->active_mask},
  };
  reason.watchdog.stuck_task_pc = stall->pc;
  reason.watchdog.stuck_task_lr = stall->lr;
  reason.watchdog.stuck_task_callback = (uint32_t)(uintptr_t)stall->work;

  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
    reboot_reason_set(&reason);
  }
}

void pbl_task_wdt_recovered(void) {
  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
  }
}

[[noreturn]] void pbl_task_wdt_expired(void) {
  reset_due_to_software_failure();
}

#ifdef CONFIG_WATCHDOG_CALLBACK
// The hardware watchdog expired without the task watchdog noticing a stall first: the task
// watchdog itself did not run, so where the CPU was tells what kept it from doing so. A reason
// already set, as the task watchdog's, is kept.
static void prv_watchdog_expiring(void *arg) {
  struct pbl_thread *thread = pbl_thread_current();
  struct pbl_thread_saved_regs regs = {};
  RebootReason reason = {.code = RebootReasonCode_Watchdog};

  // Preempted by the watchdog interrupt, the thread's live registers are read
  if (thread != nullptr) {
    pbl_thread_saved_regs(thread, &regs);
  }

  reason.watchdog.stuck_task_pc = regs.pc;
  reason.watchdog.stuck_task_lr = regs.lr;
  reboot_reason_set(&reason);
}

void task_wdt_watch_hw_expiry(void) {
  (void)watchdog_set_callback(prv_watchdog_expiring, nullptr);
}
#endif
