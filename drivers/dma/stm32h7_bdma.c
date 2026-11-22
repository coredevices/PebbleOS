/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/dma/stm32h7_bdma.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_bdma.h>
#include <hardware/stm32h7_dmamux.h>

// Priority in the channel configuration of the specifier, as for DMA1 and DMA2
#define CONFIG_PL_MASK PBL_GENMASK(17, 16)

// A channel stops once its current beat is done
#define DISABLE_TIMEOUT_US 1000U

static const struct pbl_dma_stm32h7_bdma *prv_dmac(const struct pbl_dma_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_dma_stm32h7_bdma, ctrl);
}

static uint32_t prv_flags(const struct pbl_dma_stm32h7_bdma *dmac, uint8_t chan) {
  return (pbl_readl(dmac->bdma + STM32_BDMA_ISR_OFFSET) >> BDMA_ISR_SHIFT(chan)) & BDMA_ISR_ALL;
}

static void prv_clear_flags(const struct pbl_dma_stm32h7_bdma *dmac, uint8_t chan, uint32_t flags) {
  pbl_writel(flags << BDMA_ISR_SHIFT(chan), dmac->bdma + STM32_BDMA_IFCR_OFFSET);
}

// The channel a request is bound to, or -EINVAL
static int prv_chan(const struct pbl_dma_stm32h7_bdma *dmac, uint8_t request) {
  for (uint8_t c = 0U; c < PBL_DMA_STM32H7_BDMA_NUM_CHANNELS; c++) {
    if (dmac->chans[c].request == request) {
      return c;
    }
  }

  return -EINVAL;
}

static int prv_slave_config(const struct pbl_dma_controller *ctrl, const struct pbl_dma_chan *chan,
                            const struct pbl_dma_slave_config *cfg) {
  const struct pbl_dma_stm32h7_bdma *dmac = prv_dmac(ctrl);
  int c;

  if (chan->request == 0U || chan->request != chan->channel) {
    return -EINVAL;
  }

  pbl_irq_lock();

  c = prv_chan(dmac, chan->request);
  if (c < 0) {
    c = prv_chan(dmac, 0U);
  }
  if (c < 0) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  if ((pbl_readl(dmac->bdma + STM32_BDMA_CCR_OFFSET(c)) & BDMA_CCR_EN) != 0U) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  dmac->chans[c].request = chan->request;
  dmac->chans[c].priority = (uint8_t)PBL_FIELD_GET(CONFIG_PL_MASK, chan->config);
  pbl_writel(PBL_FIELD_PREP(DMAMUX_CCR_DMAREQ_ID_MASK, chan->request),
             dmac->mux + STM32H7_DMAMUX_CCR_OFFSET(c));

  pbl_irq_unlock();

  return 0;
}

static int prv_start(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf, size_t len,
                     enum pbl_dma_dir dir, uint32_t irqs) {
  const struct pbl_dma_stm32h7_bdma *dmac = prv_dmac(ctrl);
  const struct pbl_dma_chan_state *state = &ctrl->chans[channel];
  int c = prv_chan(dmac, channel);
  uint32_t width;
  uint32_t ccr;
  uintptr_t par;

  if (c < 0) {
    return c;
  }

  if ((pbl_readl(dmac->bdma + STM32_BDMA_CCR_OFFSET(c)) & BDMA_CCR_EN) != 0U) {
    return -EBUSY;
  }

  if (dir == PBL_DMA_DEV_TO_MEM) {
    width = state->cfg.src_addr_width;
    par = state->cfg.src_addr;
  } else {
    width = state->cfg.dst_addr_width;
    par = state->cfg.dst_addr;
  }

  if ((len % width) != 0U || len / width > BDMA_CNDTR_MASK) {
    return -EINVAL;
  }

  ccr = PBL_FIELD_PREP(BDMA_CCR_PL_MASK, dmac->chans[c].priority) | BDMA_CCR_MINC |
        PBL_FIELD_PREP(BDMA_CCR_PSIZE_MASK, width >> 1) |
        PBL_FIELD_PREP(BDMA_CCR_MSIZE_MASK, width >> 1) |
        (dir == PBL_DMA_MEM_TO_DEV ? BDMA_CCR_DIR : 0U) | irqs;

  prv_clear_flags(dmac, c, BDMA_ISR_ALL);
  pbl_writel(ccr, dmac->bdma + STM32_BDMA_CCR_OFFSET(c));
  pbl_writel(len / width, dmac->bdma + STM32_BDMA_CNDTR_OFFSET(c));
  pbl_writel(par, dmac->bdma + STM32_BDMA_CPAR_OFFSET(c));
  pbl_writel((uintptr_t)buf, dmac->bdma + STM32_BDMA_CM0AR_OFFSET(c));

  if (irqs != 0U) {
    pbl_irq_clear_pending(dmac->irqs[c]);
    pbl_irq_enable(dmac->irqs[c]);
  }

  pbl_writel(ccr | BDMA_CCR_EN, dmac->bdma + STM32_BDMA_CCR_OFFSET(c));

  return 0;
}

static int prv_prep_slave_single(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                                 size_t len, enum pbl_dma_dir dir) {
  uint32_t irqs = 0U;

  if (ctrl->chans[channel].callback != nullptr) {
    irqs = BDMA_CCR_TCIE | BDMA_CCR_TEIE;
  }

  return prv_start(ctrl, channel, buf, len, dir, irqs);
}

// The half and full transfer interrupts end the two periods of the ring
static int prv_prep_dma_cyclic(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                               size_t buf_len, size_t period_len, enum pbl_dma_dir dir) {
  if (buf_len != 2U * period_len) {
    return -EINVAL;
  }

  return prv_start(ctrl, channel, buf, buf_len, dir,
                   BDMA_CCR_CIRC | BDMA_CCR_HTIE | BDMA_CCR_TCIE | BDMA_CCR_TEIE);
}

static void prv_stop(const struct pbl_dma_stm32h7_bdma *dmac, uint8_t c) {
  uintptr_t ccr = dmac->bdma + STM32_BDMA_CCR_OFFSET(c);

  pbl_irq_disable(dmac->irqs[c]);
  pbl_modifyl(BDMA_CCR_EN | BDMA_CCR_TCIE | BDMA_CCR_HTIE | BDMA_CCR_TEIE, 0U, ccr);
  (void)PBL_POLL_BUSY_US((pbl_readl(ccr) & BDMA_CCR_EN) == 0U, 1U, DISABLE_TIMEOUT_US);
  prv_clear_flags(dmac, c, BDMA_ISR_ALL);
}

static int prv_terminate(const struct pbl_dma_controller *ctrl, uint8_t channel) {
  const struct pbl_dma_stm32h7_bdma *dmac = prv_dmac(ctrl);
  int c = prv_chan(dmac, channel);

  if (c < 0) {
    return c;
  }

  prv_stop(dmac, c);

  return 0;
}

static int prv_tx_status(const struct pbl_dma_controller *ctrl, uint8_t channel, size_t *residue) {
  const struct pbl_dma_stm32h7_bdma *dmac = prv_dmac(ctrl);
  int c = prv_chan(dmac, channel);
  uint32_t ccr;

  if (c < 0) {
    return c;
  }

  ccr = pbl_readl(dmac->bdma + STM32_BDMA_CCR_OFFSET(c));
  *residue = (size_t)(pbl_readl(dmac->bdma + STM32_BDMA_CNDTR_OFFSET(c)) & BDMA_CNDTR_MASK)
             << PBL_FIELD_GET(BDMA_CCR_PSIZE_MASK, ccr);

  return 0;
}

void pbl_dma_stm32h7_bdma_irq_handler(const struct pbl_dma_stm32h7_bdma_chan_irq *irq) {
  const struct pbl_dma_stm32h7_bdma *dmac = irq->dmac;
  const struct pbl_dma_controller *ctrl = &dmac->ctrl;
  uint8_t c = irq->chan;
  uint8_t channel = dmac->chans[c].request;
  uint32_t flags = prv_flags(dmac, c);

  if ((flags & BDMA_ISR_TEIF) != 0U) {
    prv_stop(dmac, c);
    pbl_dma_complete(ctrl, channel, -EIO);
    return;
  }

  prv_clear_flags(dmac, c, flags);

  if ((pbl_readl(dmac->bdma + STM32_BDMA_CCR_OFFSET(c)) & BDMA_CCR_CIRC) != 0U) {
    if ((flags & BDMA_ISR_HTIF) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    if ((flags & BDMA_ISR_TCIF) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    return;
  }

  if ((flags & BDMA_ISR_TCIF) == 0U) {
    return;
  }

  prv_stop(dmac, c);
  pbl_dma_complete(ctrl, channel, 0);
}

int pbl_dma_stm32h7_bdma_init(const struct pbl_device *dev) {
  const struct pbl_dma_stm32h7_bdma *dmac =
      PBL_CONTAINER_OF(dev, const struct pbl_dma_stm32h7_bdma, ctrl.dev);
  int ret;

  ret = pbl_device_init(&dmac->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_clk_enable(&dmac->clk);
}

const struct pbl_dma_controller_ops pbl_dma_stm32h7_bdma_ops = {
  .slave_config = prv_slave_config,
  .prep_slave_single = prv_prep_slave_single,
  .prep_dma_cyclic = prv_prep_dma_cyclic,
  .terminate = prv_terminate,
  .tx_status = prv_tx_status,
};

#include PBL_DT_INSTANCES(st_stm32h7_dmamux_dma_st_stm32h7_bdma)
