/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/watchdog.h>

// System clock frequency for QEMU (64 MHz)
uint32_t SystemCoreClock = 64000000;

void soc_early_init(void) {
  watchdog_init();
  watchdog_start();
}
