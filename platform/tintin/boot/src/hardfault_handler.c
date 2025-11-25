/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/dbgserial.h"
#include "system/reset.h"

static void prv_hard_fault_handler_c(unsigned int *hardfault_args) {
  dbgserial_putstr("HARD FAULT");

#ifdef NO_WATCHDOG
  __asm("bkpt\n");
  while (1) continue;
#else
  system_hard_reset();
#endif
}

void HardFault_Handler(void) {
  // Grab the stack pointer, shove it into a register and call
  // the c function above.
  __asm("tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b %0\n" :: "i" (prv_hard_fault_handler_c));
}
