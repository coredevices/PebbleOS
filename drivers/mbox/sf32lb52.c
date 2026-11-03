/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mbox/sf32lb52.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_mailbox.h>

static_assert(PBL_MBOX_SF32LB52_NUM_DOORBELLS == SF32LB52_MAILBOX_NUM_CHANNELS);

static const struct pbl_mbox_sf32lb52 *prv_mbox(const struct pbl_mbox_controller *ctrl) {
  return container_of(ctrl, const struct pbl_mbox_sf32lb52, ctrl);
}

static bool prv_is_rx(const struct pbl_mbox_sf32lb52 *mbox) {
  return mbox->irqn != PBL_MBOX_SF32LB52_NO_IRQ;
}

static int prv_xlate(const struct pbl_mbox_controller *ctrl, const struct pbl_mbox_chan *chan) {
  if (chan->cells[0] >= PBL_MBOX_SF32LB52_NUM_DOORBELLS) {
    return -EINVAL;
  }

  return (int)chan->cells[0];
}

static bool prv_any_active(const struct pbl_mbox_controller *ctrl, uint8_t except) {
  for (uint8_t idx = 0U; idx < ctrl->num_chans; idx++) {
    if (idx != except && ctrl->chans[idx].cl != nullptr) {
      return true;
    }
  }

  return false;
}

static int prv_startup(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  const struct pbl_mbox_sf32lb52 *mbox = prv_mbox(ctrl);

  if (prv_is_rx(mbox)) {
    pbl_irq_enable(mbox->irqn);
  } else {
    pbl_modifyl(0U, PBL_BIT(idx), mbox->base + SF32LB52_MAILBOX_C1IER_OFFSET);
  }

  return 0;
}

static void prv_shutdown(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  const struct pbl_mbox_sf32lb52 *mbox = prv_mbox(ctrl);

  if (!prv_is_rx(mbox)) {
    pbl_modifyl(PBL_BIT(idx), 0U, mbox->base + SF32LB52_MAILBOX_C1IER_OFFSET);
  } else if (!prv_any_active(ctrl, idx)) {
    pbl_irq_disable(mbox->irqn);
  }
}

static int prv_send_data(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg) {
  const struct pbl_mbox_sf32lb52 *mbox = prv_mbox(ctrl);

  if (prv_is_rx(mbox)) {
    return -EOPNOTSUPP;
  }

  pbl_modifyl(0U, PBL_BIT(idx), mbox->base + SF32LB52_MAILBOX_C1ITR_OFFSET);

  return 0;
}

static bool prv_last_tx_done(const struct pbl_mbox_controller *ctrl, uint8_t idx) {
  return (pbl_readl(prv_mbox(ctrl)->base + SF32LB52_MAILBOX_C1ISR_OFFSET) & PBL_BIT(idx)) == 0U;
}

const struct pbl_mbox_controller_ops pbl_mbox_sf32lb52_ops = {
  .xlate = prv_xlate,
  .startup = prv_startup,
  .shutdown = prv_shutdown,
  .send_data = prv_send_data,
  .last_tx_done = prv_last_tx_done,
};

int pbl_mbox_sf32lb52_init(const struct pbl_device *dev) {
  const struct pbl_mbox_sf32lb52 *mbox =
      container_of(dev, const struct pbl_mbox_sf32lb52, ctrl.dev);

  if (mbox->clk.ctrl == nullptr) {
    return 0;
  }

  return pbl_clk_enable(&mbox->clk);
}

void pbl_mbox_sf32lb52_irq_handler(const struct pbl_mbox_sf32lb52 *mbox) {
  uint32_t status = pbl_readl(mbox->base + SF32LB52_MAILBOX_C1MISR_OFFSET);

  pbl_writel(status, mbox->base + SF32LB52_MAILBOX_C1ICR_OFFSET);

  while (status != 0U) {
    uint8_t bit = (uint8_t)__builtin_ctz(status);

    pbl_mbox_chan_received(&mbox->ctrl, bit, nullptr);
    status &= status - 1U;
  }
}
