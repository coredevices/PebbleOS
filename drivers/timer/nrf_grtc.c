/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/timer.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/nrf54l_grtc.h>

#ifdef PBL_DT_CLK_NORDIC_NRF54L15_GRTC
#include <pbl/drivers/clk/nrf54l.h>
#endif

#define GRTC PBL_DT_REG_NORDIC_NRF54L15_GRTC
#define IRQ  PBL_DT_IRQ_NORDIC_NRF54L15_GRTC

// SYSCOUNTER and interrupt group of the core, one per interrupt line
#define DOMAIN (IRQ - PBL_SOC_IRQN_GRTC_0)

#define CC_EVENT PBL_DT_PROP_GRTC_NORDIC_COMPARE_CHANNEL

#define RATE_HZ 1000000U

// SYSCOUNTER keeps the time in sleep after this many 32.768 kHz cycles without activity, and
// restarts this many before a compare
#define SLEEP_TIMEOUT 5U
#define WAKETIME      4U

#define START_DELAY_US 100U

#define MIN_DELTA 2U

#ifdef PBL_DT_CLK_NORDIC_NRF54L15_GRTC
static const struct pbl_clk s_lfclk = PBL_CLK(NRF54L_CLOCK, PBL_DT_CLK_NORDIC_NRF54L15_GRTC);
#endif

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(GRTC + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, GRTC + offset);
}

// Reading the low word latches the high one, with whether SYSCOUNTER was ready, e.g. after a
// wake-up, and whether the low word wrapped since
static uint64_t prv_counter(void) {
  uint32_t lo;
  uint32_t hi;

  do {
    lo = prv_getreg(NRF54L_GRTC_SYSCOUNTERL_OFFSET(DOMAIN));
    hi = prv_getreg(NRF54L_GRTC_SYSCOUNTERH_OFFSET(DOMAIN));
  } while ((hi & (GRTC_SYSCOUNTERH_BUSY | GRTC_SYSCOUNTERH_OVERFLOW)) != 0U);

  return ((uint64_t)(hi & GRTC_CC_CCH_MASK) << 32) | lo;
}

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return (uint32_t)prv_counter();
}

// SYSCOUNTER at or past the compare value sets the event, so a value already passed fires at once
static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  uint64_t cc = prv_counter() + delta;

  prv_putreg(NRF54L_GRTC_CC_CCL_OFFSET(CC_EVENT), (uint32_t)cc);
  // Writing CCH enables the channel
  prv_putreg(NRF54L_GRTC_CC_CCH_OFFSET(CC_EVENT), (uint32_t)(cc >> 32) & GRTC_CC_CCH_MASK);
  prv_putreg(NRF54L_GRTC_INTENSET_OFFSET(DOMAIN), GRTC_INT_COMPARE(CC_EVENT));
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_putreg(NRF54L_GRTC_INTENCLR_OFFSET(DOMAIN), GRTC_INT_COMPARE(CC_EVENT));
  prv_putreg(NRF54L_GRTC_CC_CCEN_OFFSET(CC_EVENT), 0U);
}

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_grtc_cs);
static const struct pbl_clocksource s_grtc_cs =
    PBL_CLOCKSOURCE_INIT(s_grtc_cs, "grtc", prv_init, nullptr, &s_cs_ops, 32);

PBL_DEVICE_STATE_DEFINE(s_grtc_ce);
static const struct pbl_clockevent s_grtc_ce = PBL_CLOCKEVENT_INIT(
    s_grtc_ce, "grtc", prv_init, nullptr, &s_ce_ops, MIN_DELTA, UINT32_MAX / 2U);

const struct pbl_clocksource *const pbl_system_clocksource = &s_grtc_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_grtc_ce;

static void prv_isr(void) {
  if (prv_getreg(NRF54L_GRTC_EVENTS_COMPARE_OFFSET(CC_EVENT)) == 0U) {
    return;
  }

  prv_putreg(NRF54L_GRTC_EVENTS_COMPARE_OFFSET(CC_EVENT), 0U);
  // Flush the write so the interrupt does not fire again on return.
  (void)prv_getreg(NRF54L_GRTC_EVENTS_COMPARE_OFFSET(CC_EVENT));
  prv_putreg(NRF54L_GRTC_CC_CCEN_OFFSET(CC_EVENT), 0U);

  pbl_clockevent_handler(&s_grtc_ce);
}

PBL_IRQ_CONNECT_NUM(IRQ, CONFIG_TIMER_NRF_GRTC_IRQ_PRIO, prv_isr, , 0);

// A core whose tree gives the GRTC no clock leaves the low-frequency clock to another
static int prv_start(void) {
#ifdef PBL_DT_CLK_NORDIC_NRF54L15_GRTC
  int ret;

  ret = pbl_device_init(&NRF54L_CLOCK->dev);
  if (ret != 0) {
    return ret;
  }
  ret = pbl_clk_enable(&s_lfclk);
  if (ret != 0) {
    return ret;
  }
#endif

  prv_putreg(NRF54L_GRTC_INTENCLR_OFFSET(DOMAIN), UINT32_MAX);
  prv_putreg(NRF54L_GRTC_CC_CCEN_OFFSET(CC_EVENT), 0U);
  prv_putreg(NRF54L_GRTC_EVENTS_COMPARE_OFFSET(CC_EVENT), 0U);

  // The GRTC runs on through resets other than at power-on, so a counter left running keeps on
  if ((prv_getreg(NRF54L_GRTC_MODE_OFFSET) & GRTC_MODE_SYSCOUNTEREN) == 0U) {
    pbl_modifyl(GRTC_CLKCFG_CLKSEL_MASK,
                PBL_FIELD_PREP(GRTC_CLKCFG_CLKSEL_MASK, GRTC_CLKCFG_CLKSEL_LFCLK),
                GRTC + NRF54L_GRTC_CLKCFG_OFFSET);
    prv_putreg(NRF54L_GRTC_TIMEOUT_OFFSET, SLEEP_TIMEOUT);
    prv_putreg(NRF54L_GRTC_WAKETIME_OFFSET, WAKETIME);
    prv_putreg(NRF54L_GRTC_MODE_OFFSET, GRTC_MODE_AUTOEN);
    prv_putreg(NRF54L_GRTC_TASKS_START_OFFSET, 1U);
    // SYSCOUNTER starts from the low-frequency counter, running after 3 of its cycles
    pbl_busy_wait_us(START_DELAY_US);
    prv_putreg(NRF54L_GRTC_MODE_OFFSET, GRTC_MODE_AUTOEN | GRTC_MODE_SYSCOUNTEREN);
  }

  // Keep SYSCOUNTER active while it starts, until it can be read
  prv_putreg(NRF54L_GRTC_SYSCOUNTER_ACTIVE_OFFSET(DOMAIN), GRTC_SYSCOUNTER_ACTIVE);
  (void)prv_counter();
  prv_putreg(NRF54L_GRTC_SYSCOUNTER_ACTIVE_OFFSET(DOMAIN), 0U);

  pbl_irq_enable(IRQ);

  return 0;
}

// Both devices share the counter.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;
  int ret;

  if (!s_started) {
    ret = prv_start();
    if (ret != 0) {
      return ret;
    }
    s_started = true;
  }

  if (dev == &s_grtc_cs.dev) {
    pbl_clocksource_set_hz(&s_grtc_cs, RATE_HZ);
  } else {
    pbl_clockevent_set_hz(&s_grtc_ce, RATE_HZ);
  }

  return 0;
}
