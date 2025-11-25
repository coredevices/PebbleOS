/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/dbgserial.h"
#include "util/misc.h"
#include "system/die.h"
#include "system/reset.h"

#include "misc.h"
#include "stm32f4xx.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

void hard_fault_handler_c(unsigned int* hardfault_args) {
  dbgserial_putstr("HARD FAULT");

#ifdef NO_WATCHDOG
  __BKPT();
  while (1) continue;
#else
  system_hard_reset();
#endif
}

void HardFault_Handler(void) {
  // Grab the stack pointer, shove it into a register and call
  // the c function above.
  __asm("TST LR, #4\n"
        "ITE EQ\n"
        "MRSEQ R0, MSP\n"
        "MRSNE R0, PSP\n"
        "B hard_fault_handler_c\n");
}
