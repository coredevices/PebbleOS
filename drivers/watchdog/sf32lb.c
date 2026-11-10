/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/drivers/watchdog/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_pmuc.h>
#include <hardware/sf32lb52_wdt.h>

#define WDT_CMD_TIMEOUT_US 48000U

static McuRebootReason s_cached_reset_flag;
// Registers, and counts per timeout at the rate of the clock, once the device is up
static uintptr_t s_base;
static uint32_t s_reload;

static inline uint32_t prv_wdt_getreg(uint32_t offset) {
  return pbl_readl(s_base + offset);
}

static inline void prv_wdt_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_base + offset);
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

  if (s_base == 0U) {
    return;
  }

  watchdog_stop();

  // Reset only, no interrupt
  val = prv_wdt_getreg(SF32LB52_WDT_CR_OFFSET);
  prv_wdt_putreg(SF32LB52_WDT_CR_OFFSET, val & ~WDT_CR_RESPONSE_MODE);

  prv_wdt_putreg(SF32LB52_WDT_CVR0_OFFSET, s_reload);

  pbl_modifyl(0U, PMUC_WER_WDT1, SF32LB52_PMUC_BASE + SF32LB52_PMUC_WER_OFFSET);
  pbl_modifyl(0U, HPSYS_CFG_SYSCR_WDT1_REBOOT,
              SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_SYSCR_OFFSET);
}

void watchdog_start(void) {
  if (s_base == 0U) {
    return;
  }

  prv_wdt_command(WDT_CCR_START, true);
}

void watchdog_stop(void) {
  if (s_base == 0U) {
    return;
  }

  pbl_busy_wait_us(1000U);
  prv_wdt_command(WDT_CCR_STOP, false);
  pbl_busy_wait_us(1000U);
}

void watchdog_feed(void) {
  if (s_base == 0U) {
    return;
  }

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

int pbl_watchdog_sf32lb_init(const struct pbl_device *dev) {
  const struct pbl_watchdog_sf32lb *wdt = container_of(dev, const struct pbl_watchdog_sf32lb, dev);
  uint64_t rate_mhz;
  int ret;

  ret = pbl_device_init(&wdt->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&wdt->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&wdt->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  s_reload = (uint32_t)((rate_mhz * CONFIG_WATCHDOG_TIMEOUT_MS) / 1000000U);
  if (s_reload == 0U) {
    return -EINVAL;
  }
  s_base = wdt->base;

  watchdog_init();
  watchdog_start();

  return 0;
}

#include PBL_DT_INSTANCES(sifli_sf32lb52_wdt)
