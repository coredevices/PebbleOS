/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/util/poweroff.h"
#include "drivers/display/display.h"
#include "pbl/soc/soc_poweroff.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"

NORETURN sys_poweroff(void) {
  PBL_LOG_ALWAYS("Preparing to power off");

  display_clear();
  display_set_enabled(false);

  system_reset_prepare();

  soc_poweroff();
}
