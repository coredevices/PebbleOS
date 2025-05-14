#include "drivers/watchdog.h"

void watchdog_init(void) {
}

void watchdog_start(void) {
}

void watchdog_feed(void) {
}

bool watchdog_check_reset_flag(void) {
  return 0;
}

McuRebootReason watchdog_clear_reset_flag(void) {
  McuRebootReason mcu_reboot_reason = {
    .brown_out_reset = 0,
    .pin_reset = 0,
    .power_on_reset = 1,
    .software_reset = 0,
    .independent_watchdog_reset = 0,
    .window_watchdog_reset = 0,
    .low_power_manager_reset = 0,
  };

  return mcu_reboot_reason;
}
