/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk/stm32h7.h>
#include <pbl/drivers/timer.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32_tim.h>

// The counter runs at about this rate, whatever the timer clock
#define COUNTER_HZ 1000000U

#define MIN_DELTA 2U

static const struct pbl_clk s_clk = PBL_CLK(STM32H7_RCC, STM32H7_CLK_TIM5_CK);
static uint64_t s_rate_mhz;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(PBL_DT_REG_TIMER5 + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, PBL_DT_REG_TIMER5 + offset);
}

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return prv_getreg(STM32_TIM_CNT_OFFSET);
}

// A compare value the counter passed before it was written raises the event by software, as the
// match would only come after the counter wraps.
static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  uint32_t start = prv_getreg(STM32_TIM_CNT_OFFSET);

  prv_putreg(STM32_TIM_CCR1_OFFSET, start + delta);
  prv_putreg(STM32_TIM_SR_OFFSET, ~TIM_SR_CC1IF);
  prv_putreg(STM32_TIM_DIER_OFFSET, TIM_DIER_CC1IE);
  if (prv_getreg(STM32_TIM_CNT_OFFSET) - start >= delta) {
    prv_putreg(STM32_TIM_EGR_OFFSET, TIM_EGR_CC1G);
  }
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_putreg(STM32_TIM_DIER_OFFSET, 0U);
  prv_putreg(STM32_TIM_SR_OFFSET, ~TIM_SR_CC1IF);
}

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_tim5_cs);
static const struct pbl_clocksource s_tim5_cs =
    PBL_CLOCKSOURCE_INIT(s_tim5_cs, "tim5", prv_init, nullptr, &s_cs_ops, 32);

PBL_DEVICE_STATE_DEFINE(s_tim5_ce);
static const struct pbl_clockevent s_tim5_ce = PBL_CLOCKEVENT_INIT(
    s_tim5_ce, "tim5", prv_init, nullptr, &s_ce_ops, MIN_DELTA, UINT32_MAX / 2U);

const struct pbl_clocksource *const pbl_system_clocksource = &s_tim5_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_tim5_ce;

static void prv_isr(void) {
  if ((prv_getreg(STM32_TIM_SR_OFFSET) & TIM_SR_CC1IF) == 0U) {
    return;
  }

  // One shot: the kernel programs the next event
  prv_shutdown(&s_tim5_ce);
  (void)prv_getreg(STM32_TIM_SR_OFFSET);

  pbl_clockevent_handler(&s_tim5_ce);
}

// Lowest priority, as the kernel's own interrupts.
PBL_IRQ_CONNECT(TIM5, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_isr, , 0);

// Both devices share the timer.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;
  uint64_t clk_mhz;
  uint32_t psc;
  int ret;

  if (!s_started) {
    ret = pbl_device_init(&STM32H7_RCC->dev);
    if (ret == 0) {
      ret = pbl_clk_get_rate(&s_clk, &clk_mhz);
    }
    if (ret == 0) {
      ret = pbl_clk_enable(&s_clk);
    }
    if (ret != 0) {
      return ret;
    }

    psc = (uint32_t)((clk_mhz + COUNTER_HZ * 1000ULL - 1U) / (COUNTER_HZ * 1000ULL)) - 1U;
    if (psc > TIM_PSC_MAX) {
      psc = TIM_PSC_MAX;
    }
    s_rate_mhz = clk_mhz / (psc + 1U);

    // Free running over the whole counter; the update event loads the prescaler
    prv_putreg(STM32_TIM_CR1_OFFSET, TIM_CR1_URS);
    prv_putreg(STM32_TIM_DIER_OFFSET, 0U);
    prv_putreg(STM32_TIM_PSC_OFFSET, psc);
    prv_putreg(STM32_TIM_ARR_OFFSET, UINT32_MAX);
    prv_putreg(STM32_TIM_EGR_OFFSET, TIM_EGR_UG);
    prv_putreg(STM32_TIM_SR_OFFSET, 0U);
    prv_putreg(STM32_TIM_CR1_OFFSET, TIM_CR1_URS | TIM_CR1_CEN);
    pbl_irq_enable(PBL_IRQN(TIM5));
    s_started = true;
  }

  if (dev == &s_tim5_cs.dev) {
    pbl_clocksource_set_mhz(&s_tim5_cs, s_rate_mhz);
  } else {
    pbl_clockevent_set_hz(&s_tim5_ce, (uint32_t)(s_rate_mhz / 1000U));
  }

  return 0;
}
