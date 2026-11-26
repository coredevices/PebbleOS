/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/drivers/watchdog/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <devicetree/regs.h>
#include <hardware/nrf52_power.h>
#include <hardware/nrf52_wdt.h>

static inline uint32_t prv_wdt_getreg(uint32_t offset) {
  return pbl_readl(PBL_DT_REG_WDT + offset);
}

static inline void prv_wdt_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, PBL_DT_REG_WDT + offset);
}

static inline uint32_t prv_power_getreg(uint32_t offset) {
  return pbl_readl(PBL_DT_REG_CLOCK + offset);
}

static inline void prv_power_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, PBL_DT_REG_CLOCK + offset);
}

static pbl_watchdog_callback_t s_callback;
static void *s_callback_arg;

// Two 32.768 kHz cycles before the reset: only time for the callback to note where the CPU was
static void prv_timeout_isr(void) {
  if (s_callback != nullptr) {
    s_callback(s_callback_arg);
  }
}

// Most urgent kernel priority, still masked by pbl_irq_lock(): the reset follows ~61 us after
PBL_IRQ_CONNECT(WDT, 0, prv_timeout_isr, , 0);

int watchdog_set_callback(pbl_watchdog_callback_t cb, void *arg) {
  pbl_irq_lock();
  s_callback = cb;
  s_callback_arg = arg;
  pbl_irq_unlock();

  return 0;
}

void watchdog_init(void) {
  // No effect if the bootloader already started it: a running WDT keeps its configuration.
  prv_wdt_putreg(NRF52_WDT_RREN_OFFSET, prv_wdt_getreg(NRF52_WDT_RREN_OFFSET) | WDT_RREN_RR(0));
  prv_wdt_putreg(NRF52_WDT_CRV_OFFSET, (32768ULL * CONFIG_WATCHDOG_TIMEOUT_MS) / 1000U);

  // Unlike the rest, the interrupt can be enabled while it runs
  prv_wdt_putreg(NRF52_WDT_INTENSET_OFFSET, WDT_INT_TIMEOUT);
  pbl_irq_enable(PBL_IRQN(WDT));
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

static struct pbl_mcu_reboot_reason s_cached_reset_flag;

struct pbl_mcu_reboot_reason watchdog_clear_reset_flag(void) {
  uint32_t reason = prv_power_getreg(NRF52_POWER_RESETREAS_OFFSET);
  prv_power_putreg(NRF52_POWER_RESETREAS_OFFSET, 0xffffffffU);

  s_cached_reset_flag = (struct pbl_mcu_reboot_reason){
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

struct pbl_mcu_reboot_reason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}

int pbl_watchdog_nrf5_init(const struct pbl_device *dev) {
  (void)dev;

  // Already running if the bootloader started it
  watchdog_init();
  watchdog_start();

  return 0;
}

#include PBL_DT_INSTANCES(nordic_nrf52832_wdt)
