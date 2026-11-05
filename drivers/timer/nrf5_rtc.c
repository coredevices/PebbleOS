/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/timer.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_rtc.h>

#define CC_EVENT 0U

// A compare value 0 or 1 count ahead of the counter may not trigger; one more covers an
// increment between reading the counter and writing the compare value.
#define MIN_DELTA 3U

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(NRF52_RTC1_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_RTC1_BASE + offset);
}

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return prv_getreg(NRF52_RTC_COUNTER_OFFSET);
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  uint32_t cc = (prv_getreg(NRF52_RTC_COUNTER_OFFSET) + delta) & RTC_COUNTER_MASK;

  prv_putreg(NRF52_RTC_CC_OFFSET(CC_EVENT), cc);
  prv_putreg(NRF52_RTC_INTENSET_OFFSET, RTC_INT_COMPARE(CC_EVENT));
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_putreg(NRF52_RTC_INTENCLR_OFFSET, RTC_INT_COMPARE(CC_EVENT));
}

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_rtc1_cs);
static const struct pbl_clocksource s_rtc1_cs =
    PBL_CLOCKSOURCE_INIT(s_rtc1_cs, "rtc1", prv_init, &s_cs_ops, 24);

PBL_DEVICE_STATE_DEFINE(s_rtc1_ce);
static const struct pbl_clockevent s_rtc1_ce =
    PBL_CLOCKEVENT_INIT(s_rtc1_ce, "rtc1", prv_init, &s_ce_ops, MIN_DELTA, RTC_COUNTER_MASK / 2U);

const struct pbl_clocksource *const pbl_system_clocksource = &s_rtc1_cs;
const struct pbl_clockevent *const pbl_system_clockevent = &s_rtc1_ce;

static void prv_isr(void) {
  if (prv_getreg(NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_EVENT)) == 0U) {
    return;
  }

  prv_putreg(NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_EVENT), 0U);
  // Flush the write so the interrupt does not fire again on return.
  (void)prv_getreg(NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_EVENT));

  pbl_clockevent_handler(&s_rtc1_ce);
}

// Lowest priority, as the kernel's own interrupts.
PBL_IRQ_CONNECT(RTC1, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_isr, , 0);

// Both devices share the timer.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;

  if (!s_started) {
    prv_putreg(NRF52_RTC_TASKS_STOP_OFFSET, 1U);
    prv_putreg(NRF52_RTC_INTENCLR_OFFSET, UINT32_MAX);
    prv_putreg(NRF52_RTC_EVTEN_OFFSET, 0U);
    prv_putreg(NRF52_RTC_PRESCALER_OFFSET, 0U);
    prv_putreg(NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_EVENT), 0U);
    prv_putreg(NRF52_RTC_TASKS_START_OFFSET, 1U);
    pbl_irq_enable(PBL_IRQN(RTC1));
    s_started = true;
  }

  if (dev == &s_rtc1_cs.dev) {
    pbl_clocksource_set_hz(&s_rtc1_cs, NRF52_RTC_CLOCK_HZ);
  } else {
    pbl_clockevent_set_hz(&s_rtc1_ce, NRF52_RTC_CLOCK_HZ);
  }

  return 0;
}
