/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/task_wdt/task_wdt.h>

#include <system/die.h>
#include <system/reboot_reason.h>

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
