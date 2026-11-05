/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>

#include <hardware/nrf52_clock.h>
#include <hardware/nrf52_nvmc.h>
#include <nrf.h>

static void prv_lfclk_init(void) {
  pbl_writel(CLOCK_LFCLKSRC_SRC_XTAL, NRF52_CLOCK_BASE + NRF52_CLOCK_LFCLKSRC_OFFSET);
  pbl_writel(0U, NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
  pbl_writel(1U, NRF52_CLOCK_BASE + NRF52_CLOCK_TASKS_LFCLKSTART_OFFSET);
  // TODO: time out and fall back to the calibrated RC, which needs a runtime BLE SCA. Until
  // then, the bootloader's watchdog reboots if the crystal does not start.
  while (pbl_readl(NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET) == 0U) {
  }
  pbl_writel(0U, NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
}

void pbl_soc_early_init(void) {
  SystemInit();

  prv_lfclk_init();

  pbl_writel(NVMC_ICACHECNF_CACHEEN, NRF52_NVMC_BASE + NRF52_NVMC_ICACHECNF_OFFSET);

  icache_enable();
  dcache_enable();
}
