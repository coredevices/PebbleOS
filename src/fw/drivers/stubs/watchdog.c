/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"

void watchdog_init(void) {
}

void watchdog_start(void) {
}

void watchdog_stop(void) {
}

void watchdog_feed(void) {
}

bool watchdog_check_reset_flag(void) {
  return 0;
}

static McuRebootReason s_cached_reset_flag;

McuRebootReason watchdog_clear_reset_flag(void) {
  s_cached_reset_flag = (McuRebootReason){
    .brown_out_reset = 0,
    .pin_reset = 0,
    .power_on_reset = 1,
    .software_reset = 0,
    .independent_watchdog_reset = 0,
    .window_watchdog_reset = 0,
    .low_power_manager_reset = 0,
  };

  return s_cached_reset_flag;
}

McuRebootReason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}
