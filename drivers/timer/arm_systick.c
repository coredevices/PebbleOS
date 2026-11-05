/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/timer.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/util/bits.h>

#define SYST_CSR 0xE000E010UL
#define SYST_RVR 0xE000E014UL
#define SYST_CVR 0xE000E018UL
#define ICSR     0xE000ED04UL
#define SHPR3    0xE000ED20UL

#define SYST_CSR_ENABLE    PBL_BIT(0)
#define SYST_CSR_TICKINT   PBL_BIT(1)
#define SYST_CSR_CLKSOURCE PBL_BIT(2)

#define ICSR_PENDSTCLR PBL_BIT(25)

#define SHPR3_PRI_15_SHIFT 24U
#define SHPR3_PRI_15_MASK  PBL_GENMASK(31, 24)

#define SYST_RVR_MAX PBL_BIT(24)

static void prv_stop(void) {
  pbl_writel(0U, SYST_CSR);
  pbl_writel(ICSR_PENDSTCLR, ICSR);
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  prv_stop();
  pbl_writel(delta - 1U, SYST_RVR);
  pbl_writel(0U, SYST_CVR);
  pbl_writel(SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE, SYST_CSR);
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_stop();
}

static const struct pbl_clockevent_ops s_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

static int prv_init(const struct pbl_device *dev);

PBL_DEVICE_STATE_DEFINE(s_systick);
static const struct pbl_clockevent s_systick =
    PBL_CLOCKEVENT_INIT(s_systick, "systick", prv_init, &s_ops, 64U, SYST_RVR_MAX);

const struct pbl_clockevent *const pbl_system_clockevent = &s_systick;

static int prv_init(const struct pbl_device *dev) {
  prv_stop();
  pbl_modifyl(SHPR3_PRI_15_MASK, (uint32_t)PBL_IRQ_PRIO_KERNEL << SHPR3_PRI_15_SHIFT, SHPR3);
  pbl_clockevent_set_hz(&s_systick, pbl_cycles_per_sec());

  return 0;
}

void SysTick_Handler(void) {
  // Reloaded and still counting: one shot only.
  pbl_writel(0U, SYST_CSR);
  pbl_clockevent_handler(&s_systick);
}
