/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/stm32h7.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/drivers/watchdog/stm32_iwdg.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_iwdg.h>
#include <hardware/stm32h7_rcc.h>

// The prescaler and reload take up to 5 LSI cycles, about 160 us, to reach the counter domain
#define UPDATE_TIMEOUT_US 1000U

static const struct pbl_watchdog_stm32_iwdg *s_wdt;
static uint32_t s_pr;
static uint32_t s_rlr;
static struct pbl_mcu_reboot_reason s_cached_reset_flag;

static inline void prv_key(uint32_t key) {
  pbl_writel(key, s_wdt->base + STM32_IWDG_KR_OFFSET);
}

void watchdog_init(void) {
  if (s_wdt == nullptr) {
    return;
  }

  prv_key(IWDG_KR_UNLOCK);
  pbl_writel(s_pr, s_wdt->base + STM32_IWDG_PR_OFFSET);
  pbl_writel(s_rlr, s_wdt->base + STM32_IWDG_RLR_OFFSET);
  (void)PBL_POLL_BUSY_US(
      (pbl_readl(s_wdt->base + STM32_IWDG_SR_OFFSET) & (IWDG_SR_PVU | IWDG_SR_RVU)) == 0U, 10U,
      UPDATE_TIMEOUT_US);
  prv_key(IWDG_KR_RELOAD);
}

void watchdog_start(void) {
  if (s_wdt == nullptr) {
    return;
  }

  prv_key(IWDG_KR_START);
}

void watchdog_feed(void) {
  if (s_wdt == nullptr) {
    return;
  }

  prv_key(IWDG_KR_RELOAD);
}

static uint32_t prv_reset_flags(bool clear) {
  if (s_wdt == nullptr) {
    return 0U;
  }

  return pbl_clk_stm32h7_reset_flags(s_wdt->clk.ctrl, clear);
}

bool watchdog_check_reset_flag(void) {
  return (prv_reset_flags(false) & RCC_RSR_IWDG1RSTF) != 0U;
}

struct pbl_mcu_reboot_reason watchdog_clear_reset_flag(void) {
  uint32_t rsr = prv_reset_flags(true);

  s_cached_reset_flag = (struct pbl_mcu_reboot_reason){
    .brown_out_reset = (rsr & RCC_RSR_BORRSTF) != 0U,
    .pin_reset = (rsr & RCC_RSR_PINRSTF) != 0U,
    .power_on_reset = (rsr & RCC_RSR_PORRSTF) != 0U,
    .software_reset = (rsr & RCC_RSR_SFT1RSTF) != 0U,
    .independent_watchdog_reset = (rsr & RCC_RSR_IWDG1RSTF) != 0U,
    .window_watchdog_reset = (rsr & RCC_RSR_WWDG1RSTF) != 0U,
    .low_power_manager_reset = (rsr & RCC_RSR_LPWR1RSTF) != 0U,
  };

  return s_cached_reset_flag;
}

struct pbl_mcu_reboot_reason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}

int pbl_watchdog_stm32_iwdg_init(const struct pbl_device *dev) {
  const struct pbl_watchdog_stm32_iwdg *wdt =
      PBL_CONTAINER_OF(dev, const struct pbl_watchdog_stm32_iwdg, dev);
  uint64_t rate_mhz;
  uint64_t counts;
  uint32_t pr = 0U;
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

  // The counter runs at the clock divided by 4 << PR
  counts = (rate_mhz * CONFIG_WATCHDOG_TIMEOUT_MS) / 1000000U / 4U;
  while (counts > IWDG_RLR_MAX + 1U && pr < IWDG_PR_MAX) {
    counts /= 2U;
    pr++;
  }
  if (counts == 0U || counts > IWDG_RLR_MAX + 1U) {
    return -EINVAL;
  }

  s_pr = pr;
  s_rlr = (uint32_t)counts - 1U;
  s_wdt = wdt;

  watchdog_start();
  watchdog_init();

  return 0;
}

#include PBL_DT_INSTANCES(st_stm32_iwdg)
