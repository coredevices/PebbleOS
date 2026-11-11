/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_clock.h>

#define HFCLKSTAT_XTAL_RUNNING (CLOCK_HFCLKSTAT_SRC_XTAL | CLOCK_HFCLKSTAT_STATE)

static uint8_t prv_refcnt;

void clocksource_hfxo_request(void) {
  pbl_irq_lock();

  PBL_ASSERT(prv_refcnt < UINT8_MAX, "HFXO refcount overflow");

  if (prv_refcnt == 0U && (pbl_readl(NRF52_CLOCK_BASE + NRF52_CLOCK_HFCLKSTAT_OFFSET) &
                           HFCLKSTAT_XTAL_RUNNING) != HFCLKSTAT_XTAL_RUNNING) {
    pbl_writel(0U, NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_HFCLKSTARTED_OFFSET);
    pbl_writel(1U, NRF52_CLOCK_BASE + NRF52_CLOCK_TASKS_HFCLKSTART_OFFSET);
    while (pbl_readl(NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_HFCLKSTARTED_OFFSET) == 0U) {
    }
  }

  prv_refcnt++;

  pbl_irq_unlock();
}

void clocksource_hfxo_release(void) {
  pbl_irq_lock();

  PBL_ASSERT(prv_refcnt != 0U, "HFXO refcount underflow");

  prv_refcnt--;
  if (prv_refcnt == 0U) {
    pbl_writel(1U, NRF52_CLOCK_BASE + NRF52_CLOCK_TASKS_HFCLKSTOP_OFFSET);
  }

  pbl_irq_unlock();
}
