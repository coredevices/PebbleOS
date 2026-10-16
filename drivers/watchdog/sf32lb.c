/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_pmuc.h>
#include <hardware/sf32lb52_wdt.h>

#define WDT_CMD_TIMEOUT_US 48000U

static McuRebootReason s_cached_reset_flag;

static inline uint32_t prv_wdt_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_WDT1_BASE + offset);
}

static inline void prv_wdt_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_WDT1_BASE + offset);
}

static inline uint32_t prv_pmuc_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_PMUC_BASE + offset);
}

static inline void prv_pmuc_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_PMUC_BASE + offset);
}

static void prv_wdt_command(uint32_t cmd, bool active) {
  prv_wdt_putreg(SF32LB52_WDT_CCR_OFFSET, cmd);

  (void)PBL_POLL_BUSY_US(
      ((prv_wdt_getreg(SF32LB52_WDT_SR_OFFSET) & WDT_SR_WDT_ACTIVE) != 0U) == active, 1U,
      WDT_CMD_TIMEOUT_US);
}

void watchdog_init(void) {
  uint32_t val;

  watchdog_stop();

  // Reset only, no interrupt
  val = prv_wdt_getreg(SF32LB52_WDT_CR_OFFSET);
  prv_wdt_putreg(SF32LB52_WDT_CR_OFFSET, val & ~WDT_CR_RESPONSE_MODE);

  // On PebbleOS, we use RC32K as WDT clock source.
  prv_wdt_putreg(SF32LB52_WDT_CVR0_OFFSET, 32U * CONFIG_WATCHDOG_TIMEOUT_MS);

  pbl_modifyl(0U, PMUC_WER_WDT1, SF32LB52_PMUC_BASE + SF32LB52_PMUC_WER_OFFSET);
  pbl_modifyl(0U, HPSYS_CFG_SYSCR_WDT1_REBOOT,
              SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_SYSCR_OFFSET);
}

void watchdog_start(void) {
  prv_wdt_command(WDT_CCR_START, true);
}

void watchdog_stop(void) {
  pbl_busy_wait_us(1000U);
  prv_wdt_command(WDT_CCR_STOP, false);
  pbl_busy_wait_us(1000U);
}

void watchdog_feed(void) {
  prv_wdt_command(WDT_CCR_START, true);
}

bool watchdog_check_reset_flag(void) {
  return (prv_pmuc_getreg(SF32LB52_PMUC_WSR_OFFSET) & PMUC_WSR_WDT1) != 0U;
}

McuRebootReason watchdog_clear_reset_flag(void) {
  uint32_t wsr = prv_pmuc_getreg(SF32LB52_PMUC_WSR_OFFSET);
  prv_pmuc_putreg(SF32LB52_PMUC_WCR_OFFSET, 0xffffffffU);

  s_cached_reset_flag = (McuRebootReason){
    .brown_out_reset = 0,
    .pin_reset = (wsr & (PMUC_WSR_PIN0 | PMUC_WSR_PIN1)) != 0U,
    .power_on_reset = 0,
    .software_reset = 0,
    .independent_watchdog_reset = 0,
    .window_watchdog_reset = (wsr & PMUC_WSR_WDT1) != 0U,
    .low_power_manager_reset = 0,
  };

  return s_cached_reset_flag;
}

McuRebootReason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}
