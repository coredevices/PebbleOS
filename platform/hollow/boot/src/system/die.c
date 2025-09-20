#include "drivers/dbgserial.h"
#include "system/reset.h"
#include "system/passert.h"

void reset_due_to_software_failure(void) {
#if defined(NO_WATCHDOG)
  // Don't reset right away, leave it in a state we can inspect

  while (1) {
    BREAKPOINT;
  }
#endif

  dbgserial_putstr("Software failure; resetting!");
  system_reset();
}
