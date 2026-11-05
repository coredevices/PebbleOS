/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/timer.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_lptim.h>

// A few RC10K cycles
#define COUNTRST_TIMEOUT_US 1000U

// A compare value at the counter may not match; one more count covers an increment between
// reading the counter and writing the compare value.
#define MIN_DELTA 2U

static uint64_t s_rate_mhz;

static void prv_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz);

static struct pbl_clk_notifier s_rc10k_nb = {
  .clk = PBL_CLK(SF32LB_RC10K, SF32LB52_CLK_RC10K),
  .fn = prv_rate_changed,
};

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_LPTIM1_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_LPTIM1_BASE + offset);
}

// The counter runs on the low-power clock: read until two reads agree.
static uint32_t prv_read(const struct pbl_clocksource *cs) {
  uint32_t cnt;
  uint32_t prev;

  cnt = prv_getreg(SF32LB52_LPTIM_CNT_OFFSET);
  do {
    prev = cnt;
    cnt = prv_getreg(SF32LB52_LPTIM_CNT_OFFSET);
  } while (cnt != prev);

  return cnt;
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  uint32_t cmp = (prv_read(nullptr) + delta) & LPTIM_ARR_MASK;

  prv_putreg(SF32LB52_LPTIM_CMP_OFFSET, cmp);
  prv_putreg(SF32LB52_LPTIM_IER_OFFSET, LPTIM_IER_OCIE | LPTIM_IER_OCWE);
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_putreg(SF32LB52_LPTIM_IER_OFFSET, 0U);
}

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_lptim1_cs);
static const struct pbl_clocksource s_lptim1_cs =
    PBL_CLOCKSOURCE_INIT(s_lptim1_cs, "lptim1", prv_init, &s_cs_ops, 24);

PBL_DEVICE_STATE_DEFINE(s_lptim1_ce);
static const struct pbl_clockevent s_lptim1_ce =
    PBL_CLOCKEVENT_INIT(s_lptim1_ce, "lptim1", prv_init, &s_ce_ops, MIN_DELTA, LPTIM_ARR_MASK / 2U);

const struct pbl_clocksource *const pbl_system_clocksource = &s_lptim1_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_lptim1_ce;

// Follows the RC10K calibration.
static void prv_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz) {
  s_rate_mhz = rate_mhz;
  pbl_clocksource_set_mhz(&s_lptim1_cs, rate_mhz);
  pbl_clockevent_set_hz(&s_lptim1_ce, (uint32_t)(rate_mhz / 1000U));
}

static void prv_isr(void) {
  if ((prv_getreg(SF32LB52_LPTIM_ISR_OFFSET) & LPTIM_ISR_OC) == 0U) {
    return;
  }

  prv_putreg(SF32LB52_LPTIM_ICR_OFFSET, LPTIM_ICR_OCCLR | LPTIM_ICR_WKUPCLR);

  pbl_clockevent_handler(&s_lptim1_ce);
}

// Lowest priority, as the kernel's own interrupts.
PBL_IRQ_CONNECT(LPTIM1, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_isr, , 0);

// Both devices share the timer.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;
  int ret;

  if (!s_started) {
    ret = pbl_device_init(&SF32LB_RC10K->dev);
    if (ret == 0) {
      ret = pbl_clk_get_rate(&s_rc10k_nb.clk, &s_rate_mhz);
    }
    if (ret != 0) {
      return ret;
    }
    pbl_clk_notifier_register(&s_rc10k_nb);

    // Free running over the whole counter, on the RC10K clock.
    prv_putreg(SF32LB52_LPTIM_CR_OFFSET, 0U);
    prv_putreg(SF32LB52_LPTIM_IER_OFFSET, 0U);
    prv_putreg(SF32LB52_LPTIM_CFGR_OFFSET, 0U);
    prv_putreg(SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_COUNTRST);
    ret = PBL_POLL_BUSY_US((prv_getreg(SF32LB52_LPTIM_CR_OFFSET) & LPTIM_CR_COUNTRST) == 0U, 10U,
                           COUNTRST_TIMEOUT_US);
    if (ret != 0) {
      return ret;
    }
    prv_putreg(SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE);
    prv_putreg(SF32LB52_LPTIM_ARR_OFFSET, LPTIM_ARR_MASK);
    prv_putreg(SF32LB52_LPTIM_ICR_OFFSET, LPTIM_ICR_OCCLR | LPTIM_ICR_WKUPCLR);
    prv_putreg(SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT);
    pbl_irq_enable(PBL_IRQN(LPTIM1));
    s_started = true;
  }

  if (dev == &s_lptim1_cs.dev) {
    pbl_clocksource_set_mhz(&s_lptim1_cs, s_rate_mhz);
  } else {
    pbl_clockevent_set_hz(&s_lptim1_ce, (uint32_t)(s_rate_mhz / 1000U));
  }

  return 0;
}
