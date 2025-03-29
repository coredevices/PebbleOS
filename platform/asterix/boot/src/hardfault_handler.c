#include "drivers/dbgserial.h"
#include "system/die.h"
#include "system/reset.h"

static void prv_hard_fault_handler_c(unsigned int *hardfault_args) {
  (void)hardfault_args;
  dbgserial_putstr("HARD FAULT");

#ifdef NO_WATCHDOG
  reset_due_to_software_failure();
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
