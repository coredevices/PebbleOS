/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "die.h"
#include "drivers/dbgserial.h"
#include "system/reset.h"
#include "system/passert.h"

NORETURN reset_due_to_software_failure(void) {
#if defined(NO_WATCHDOG)
  // Don't reset right away, leave it in a state we can inspect

  while (1) {
    BREAKPOINT;
  }
#endif

  dbgserial_putstr("Software failure; resetting!");
  system_reset();
}
