#include "system/reset.h"

#include "drivers/display.h"

#include <nrfx.h>

void system_reset(void) {
  display_prepare_for_reset();
  system_hard_reset();
}

void system_hard_reset(void) {
  NVIC_SystemReset();
  __builtin_unreachable();
}
