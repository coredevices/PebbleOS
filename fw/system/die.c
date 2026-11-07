/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include <pbl/coredump/coredump.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/logging/backend.h>

#include <logging/logging_private.h>
#include <system/bootbits.h>
#include <system/reboot_reason.h>

#ifdef CONFIG_CONSOLE
#include <pbl/console/console.h>
#endif

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

[[noreturn]] void reset_with_core_dump(bool is_forced) {
#ifdef CONFIG_COREDUMP
  if (is_forced) {
    RebootReason reason = {RebootReasonCode_ForcedCoreDump, 0};
    reboot_reason_set(&reason);
  }

  pbl_coredump_trigger(is_forced);

  RebootReason reason = {RebootReasonCode_CoreDumpEntryFailed, 0};
  reboot_reason_set(&reason);
#ifdef CONFIG_CONSOLE
  pbl_console_flush();
#endif
  pbl_reboot_emergency();
#elif defined(CONFIG_ARCH_POSIX)
  // Stop where a debugger or the host's crash reporter can look at it.
  fprintf(stderr, "core dump requested, aborting\n");
  abort();
#else
  pbl_reboot_emergency();
#endif
}

static void prv_restart(enum pbl_sys_off_mode mode, void *user_data) {
  if (boot_bit_test(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED)) {
    reset_with_core_dump(false /* is_forced */);
  }
}

PBL_SYS_OFF_HANDLER_DEFINE(core_dump, PBL_SYS_OFF_RESTART, PBL_SYS_OFF_PRIO_HIGH, prv_restart,
                           nullptr);
