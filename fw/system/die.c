/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/logging/backend.h>

#include <kernel/core_dump.h>
#include <logging/logging_private.h>
#include <system/bootbits.h>

void prepare_for_software_failure(void) {
  pbl_log_panic();

#ifndef CONFIG_MFG
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
#endif
}

[[noreturn]] void reset_due_to_software_failure(void) {
  prepare_for_software_failure();

#ifndef CONFIG_WATCHDOG
  // Don't reset right away, leave it in a state we can inspect

  pbl_irq_lock();
  while (1) {
    continue;
  }
#endif

  PBL_LOG_FROM_FAULT_HANDLER("Resetting!");
  pbl_reboot();
}

static void prv_restart(enum pbl_sys_off_mode mode, void *user_data) {
  if (boot_bit_test(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED)) {
    core_dump_reset(false /* is_forced */);
  }
}

PBL_SYS_OFF_HANDLER_DEFINE(core_dump, PBL_SYS_OFF_RESTART, PBL_SYS_OFF_PRIO_HIGH, prv_restart,
                           nullptr);
