/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>

#include <pbl/drivers/clk/stm32h7.h>
#include <pbl/drivers/timer.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>

#include <devicetree/regs.h>
#include <hardware/stm32_lptim.h>

// A few cycles of the counter clock
#define SYNC_TIMEOUT_US 1000U

// A compare value takes a few counts to reach the counter domain; one closer than this may be
// passed before it does, and is raised by software
#define CMP_LATENCY 3U
#define MIN_DELTA   (CMP_LATENCY + 2U)

/*
 * Clearing a flag outside the interrupt handler, or one whose interrupt is disabled, may leave
 * the interrupt line stuck active (errata of the STM32H7). Only the compare match interrupt is
 * enabled, and only its handler clears it: CMPOK is never cleared, and a compare value is written
 * once the counter moved on far enough for the one before to be taken.
 */

#define LPTIM_BASE PBL_DT_REG_ST_STM32_LPTIMER_TIMER
#define LPTIM_IRQN PBL_DT_IRQ_ST_STM32_LPTIMER_TIMER

static const struct pbl_clk s_clk = PBL_CLK(STM32H7_RCC, PBL_DT_CLK_ST_STM32_LPTIMER_TIMER);
static uint64_t s_rate_mhz;
// Count at which the last compare value was written, if any
static bool s_cmp_written;
static uint32_t s_cmp_at;
// The event programmed: from a count, a number of counts on, or raised by software
static uint32_t s_start;
static uint32_t s_delta;
static bool s_forced;
// The compare match fires at every wrap: only an armed one may be an event
static atomic_bool s_armed;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(LPTIM_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, LPTIM_BASE + offset);
}

// The counter runs on the low-power clock: read until two reads agree
static uint32_t prv_read(const struct pbl_clocksource *cs) {
  uint32_t cnt;
  uint32_t prev;

  cnt = prv_getreg(STM32_LPTIM_CNT_OFFSET);
  do {
    prev = cnt;
    cnt = prv_getreg(STM32_LPTIM_CNT_OFFSET);
  } while (cnt != prev);

  return cnt;
}

static uint32_t prv_elapsed(uint32_t since) {
  return (prv_read(nullptr) - since) & LPTIM_CNT_MASK;
}

// The compare match holds while the counter is at or past the compare value: one past the top
// of the counter would match until it wraps. An event due later is raised at the top, for the
// kernel to program what is left.
static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  if (s_cmp_written) {
    (void)PBL_POLL_BUSY_US(prv_elapsed(s_cmp_at) >= CMP_LATENCY, 1U, SYNC_TIMEOUT_US);
  }

  s_start = prv_read(nullptr);
  s_delta = PBL_MIN(delta, LPTIM_CNT_MASK - s_start);
  prv_putreg(STM32_LPTIM_CMP_OFFSET, s_start + s_delta);
  s_cmp_at = prv_read(nullptr);
  s_cmp_written = true;
  s_forced = ((s_cmp_at - s_start) & LPTIM_CNT_MASK) + CMP_LATENCY >= s_delta;
  atomic_store(&s_armed, true);

  if (s_forced) {
    pbl_irq_set_pending(LPTIM_IRQN);
  }
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  atomic_store(&s_armed, false);
}

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_lptim_cs);
static const struct pbl_clocksource s_lptim_cs =
    PBL_CLOCKSOURCE_INIT(s_lptim_cs, "lptim", prv_init, nullptr, &s_cs_ops, 16);

PBL_DEVICE_STATE_DEFINE(s_lptim_ce);
static const struct pbl_clockevent s_lptim_ce = PBL_CLOCKEVENT_INIT(
    s_lptim_ce, "lptim", prv_init, nullptr, &s_ce_ops, MIN_DELTA, LPTIM_CNT_MASK / 2U);

const struct pbl_clocksource *const pbl_system_clocksource = &s_lptim_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_lptim_ce;

static void prv_isr(void) {
  if ((prv_getreg(STM32_LPTIM_ISR_OFFSET) & LPTIM_ISR_CMPM) != 0U) {
    prv_putreg(STM32_LPTIM_ICR_OFFSET, LPTIM_ISR_CMPM);
  }

  // A match the counter has not reached yet is that of an earlier compare value
  if (!atomic_load(&s_armed) || (!s_forced && prv_elapsed(s_start) < s_delta)) {
    return;
  }

  atomic_store(&s_armed, false);
  pbl_clockevent_handler(&s_lptim_ce);
}

PBL_IRQ_CONNECT_NUM(LPTIM_IRQN, CONFIG_TIMER_STM32_LPTIM_IRQ_PRIO, prv_isr, , 0);

// Both devices share the timer.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;
  int ret;

  if (!s_started) {
    ret = pbl_device_init(&STM32H7_RCC->dev);
    if (ret == 0) {
      ret = pbl_clk_enable(&s_clk);
    }
    if (ret == 0) {
      ret = pbl_clk_get_rate(&s_clk, &s_rate_mhz);
    }
    if (ret != 0) {
      return ret;
    }

    // Free running over the whole counter, undivided; the interrupt is only set while disabled
    prv_putreg(STM32_LPTIM_CR_OFFSET, 0U);
    prv_putreg(STM32_LPTIM_CFGR_OFFSET, 0U);
    prv_putreg(STM32_LPTIM_IER_OFFSET, LPTIM_ISR_CMPM);
    prv_putreg(STM32_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE);
    prv_putreg(STM32_LPTIM_ARR_OFFSET, LPTIM_CNT_MASK);
    ret = PBL_POLL_BUSY_US((prv_getreg(STM32_LPTIM_ISR_OFFSET) & LPTIM_ISR_ARROK) != 0U, 1U,
                           SYNC_TIMEOUT_US);
    if (ret != 0) {
      return ret;
    }
    // Not counting yet, nothing sets a flag while they are cleared
    prv_putreg(STM32_LPTIM_ICR_OFFSET, LPTIM_ISR_ALL);
    prv_putreg(STM32_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT);
    pbl_irq_enable(LPTIM_IRQN);
    s_started = true;
  }

  if (dev == &s_lptim_cs.dev) {
    pbl_clocksource_set_mhz(&s_lptim_cs, s_rate_mhz);
  } else {
    pbl_clockevent_set_hz(&s_lptim_ce, (uint32_t)(s_rate_mhz / 1000U));
  }

  return 0;
}
