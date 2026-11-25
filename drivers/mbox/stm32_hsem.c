/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/mbox.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/exti.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_hsem.h>

#define NUM_SEMAPHORES 32U

// The interrupt registers of this core and of the other one, and the ID this core takes with
#ifdef CONFIG_CPU_CORTEX_M4
#define OWN(reg)   STM32H7_HSEM_C2##reg##_OFFSET
#define OTHER(reg) STM32H7_HSEM_C1##reg##_OFFSET
#define COREID     HSEM_COREID_CM4
#else
#define OWN(reg)   STM32H7_HSEM_C1##reg##_OFFSET
#define OTHER(reg) STM32H7_HSEM_C2##reg##_OFFSET
#define COREID     HSEM_COREID_CM7
#endif

struct pbl_mbox_stm32_hsem {
  struct pbl_mbox_controller ctrl;
  uintptr_t base;
  pbl_irq_t irqn;
  // Line of the EXTI the interrupt comes through
  uint32_t wakeup;
  struct pbl_clk clk;
};

static const struct pbl_mbox_stm32_hsem *prv_hsem(const struct pbl_mbox_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_mbox_stm32_hsem, ctrl);
}

static int prv_xlate(const struct pbl_mbox_controller *ctrl, const struct pbl_mbox_chan *chan) {
  if (chan->cells[0] >= NUM_SEMAPHORES) {
    return -EINVAL;
  }

  return (int)chan->cells[0];
}

static int prv_startup(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  const struct pbl_mbox_stm32_hsem *hsem = prv_hsem(ctrl);

  pbl_irq_lock();
  pbl_writel(PBL_BIT(idx), hsem->base + OWN(ICR));
  pbl_modifyl(0U, PBL_BIT(idx), hsem->base + OWN(IER));
  pbl_irq_unlock();

  pbl_irq_enable(hsem->irqn);

  return 0;
}

static void prv_shutdown(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  pbl_modifyl(PBL_BIT(idx), 0U, prv_hsem(ctrl)->base + OWN(IER));
}

// Taking and releasing the semaphore rings the cores that enabled it, this one excepted
static int prv_send_data(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg) {
  const struct pbl_mbox_stm32_hsem *hsem = prv_hsem(ctrl);
  const uint32_t taken = HSEM_R_LOCK | PBL_FIELD_PREP(HSEM_R_COREID_MASK, COREID);
  uint32_t ier;
  int ret = 0;

  pbl_irq_lock();
  ier = pbl_readl(hsem->base + OWN(IER));
  pbl_writel(ier & ~PBL_BIT(idx), hsem->base + OWN(IER));

  if ((pbl_readl(hsem->base + STM32H7_HSEM_RLR_OFFSET(idx)) & ~HSEM_R_PROCID_MASK) != taken) {
    ret = -EBUSY;
  } else {
    pbl_writel(PBL_FIELD_PREP(HSEM_R_COREID_MASK, COREID), hsem->base + STM32H7_HSEM_R_OFFSET(idx));
    pbl_writel(PBL_BIT(idx), hsem->base + OWN(ICR));
  }

  pbl_writel(ier, hsem->base + OWN(IER));
  pbl_irq_unlock();

  return ret;
}

// The other core took the doorbell once it cleared it
static bool prv_last_tx_done(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  return (pbl_readl(prv_hsem(ctrl)->base + OTHER(ISR)) & PBL_BIT(idx)) == 0U;
}

static const struct pbl_mbox_controller_ops s_ops = {
  .xlate = prv_xlate,
  .startup = prv_startup,
  .shutdown = prv_shutdown,
  .send_data = prv_send_data,
  .last_tx_done = prv_last_tx_done,
};

static void prv_isr(const struct pbl_mbox_stm32_hsem *hsem) {
  uint32_t pending = pbl_readl(hsem->base + OWN(MISR));

  pbl_writel(pending, hsem->base + OWN(ICR));

  for (uint8_t idx = 0U; idx < hsem->ctrl.num_chans; idx++) {
    if ((pending & PBL_BIT(idx)) != 0U) {
      pbl_mbox_chan_received(&hsem->ctrl, idx, nullptr);
    }
  }
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_mbox_stm32_hsem *hsem =
      PBL_CONTAINER_OF(dev, const struct pbl_mbox_stm32_hsem, ctrl.dev);
  int ret;

  ret = pbl_clk_enable(&hsem->clk);
  if (ret != 0) {
    return ret;
  }

  // The other core rings this one stopped
  pbl_soc_stm32h7_wakeup_enable(hsem->wakeup);

  return 0;
}

#define PBL_MBOX_STM32_HSEM_DEFINE(sym, _name, _base, _clk, _num_chans, _irq, _wakeup)    \
  static_assert((_num_chans) <= NUM_SEMAPHORES);                                          \
  PBL_MBOX_CONTROLLER_STATE_DEFINE(sym, _num_chans);                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_mbox_stm32_hsem, struct pbl_mbox_controller, ctrl){ \
    .ctrl = PBL_MBOX_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops, 1U),          \
    .base = (_base),                                                                      \
    .irqn = (_irq),                                                                       \
    .wakeup = (_wakeup),                                                                  \
    .clk = _clk,                                                                          \
  };                                                                                      \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_MBOX_STM32_HSEM_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(st_stm32_hwspinlock)
