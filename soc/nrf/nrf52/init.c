/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>

#include <hardware/nrf52_nvmc.h>
#include <nrf.h>

void pbl_soc_early_init(void) {
  SystemInit();

  pbl_writel(NVMC_ICACHECNF_CACHEEN, NRF52_NVMC_BASE + NRF52_NVMC_ICACHECNF_OFFSET);

  icache_enable();
  dcache_enable();
}
