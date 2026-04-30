/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "setup.h"

#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"

void enable_mcu_debugging(void) {
}

void disable_mcu_debugging(void) {
}

void command_low_power_debug(char *cmd) {
  bool low_power_debug_on = (strcmp(cmd, "on") == 0);

  if (low_power_debug_on) {
    enable_mcu_debugging();
  } else {
    disable_mcu_debugging();
  }
}
