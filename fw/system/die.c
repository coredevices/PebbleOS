/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/irq.h>
#include <pbl/logging/backend.h>

#include <logging/logging_private.h>
#include <system/bootbits.h>
#include <system/reset.h>

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
  system_reset();
}
