/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/init.h>
#include <pbl/kernel/time.h>

void pbl_soc_early_init(void) {
  // The emulated CPU counts cycles at 64 MHz
  pbl_cycles_per_sec_set(64000000U);

  watchdog_init();
  watchdog_start();
}
