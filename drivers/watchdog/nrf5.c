/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_power.h>
#include <hardware/nrf52_wdt.h>

static inline uint32_t prv_wdt_getreg(uint32_t offset) {
  return pbl_readl(NRF52_WDT_BASE + offset);
}

static inline void prv_wdt_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_WDT_BASE + offset);
}

static inline uint32_t prv_power_getreg(uint32_t offset) {
  return pbl_readl(NRF52_POWER_BASE + offset);
}

static inline void prv_power_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_POWER_BASE + offset);
}

void watchdog_init(void) {
  // No effect if the bootloader already started it: a running WDT keeps its configuration.
  prv_wdt_putreg(NRF52_WDT_RREN_OFFSET, prv_wdt_getreg(NRF52_WDT_RREN_OFFSET) | WDT_RREN_RR(0));
  prv_wdt_putreg(NRF52_WDT_CRV_OFFSET, (32768ULL * CONFIG_WATCHDOG_TIMEOUT_MS) / 1000U);
}

void watchdog_start(void) {
  prv_wdt_putreg(NRF52_WDT_TASKS_START_OFFSET, 1U);
}

void watchdog_feed(void) {
  prv_wdt_putreg(NRF52_WDT_RR_OFFSET(0), WDT_RR_RELOAD);
}

bool watchdog_check_reset_flag(void) {
  return (prv_power_getreg(NRF52_POWER_RESETREAS_OFFSET) & POWER_RESETREAS_DOG) != 0U;
}

static McuRebootReason s_cached_reset_flag;

McuRebootReason watchdog_clear_reset_flag(void) {
  uint32_t reason = prv_power_getreg(NRF52_POWER_RESETREAS_OFFSET);
  prv_power_putreg(NRF52_POWER_RESETREAS_OFFSET, 0xffffffffU);

  s_cached_reset_flag = (McuRebootReason){
    .brown_out_reset = 0,
    .pin_reset = (reason & POWER_RESETREAS_RESETPIN) != 0U,
    .power_on_reset = (reason & POWER_RESETREAS_VBUS) != 0U,
    .software_reset = (reason & POWER_RESETREAS_SREQ) != 0U,
    .independent_watchdog_reset = (reason & POWER_RESETREAS_DOG) != 0U,
    .window_watchdog_reset = 0,
    .low_power_manager_reset = 0,
  };

  return s_cached_reset_flag;
}

McuRebootReason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}
