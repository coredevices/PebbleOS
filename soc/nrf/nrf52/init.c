/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/init.h>
#include <pbl/mcu/cache.h>

#include <nrf.h>

void pbl_soc_early_init(void) {
  SystemInit();

  icache_enable();
  dcache_enable();
}
