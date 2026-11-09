/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_dmac.h>

#define MAX_REQUEST 0x3fU

static_assert(PBL_DMA_SF32LB52_NUM_CHANNELS == SF32LB52_DMAC_NUM_CHANNELS);

// The MPI (XIP flash) code bus window, which the DMAC reaches through the system bus instead
#define MPI_CBUS_START  0x10000000U
#define MPI_CBUS_END    0x20000000U
#define MPI_CBUS_2_SBUS 0x50000000U

static const struct pbl_dma_sf32lb52 *prv_dmac(const struct pbl_dma_controller *ctrl) {
  return container_of(ctrl, const struct pbl_dma_sf32lb52, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_dma_controller *ctrl, uint32_t offset) {
  return pbl_readl(prv_dmac(ctrl)->base + offset);
}

static inline void prv_putreg(const struct pbl_dma_controller *ctrl, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, prv_dmac(ctrl)->base + offset);
}

static uint32_t prv_bus_addr(uintptr_t addr) {
  if (addr >= MPI_CBUS_START && addr < MPI_CBUS_END) {
    return addr + MPI_CBUS_2_SBUS;
  }

  return addr;
}

static int prv_slave_config(const struct pbl_dma_controller *ctrl, const struct pbl_dma_chan *chan,
                            const struct pbl_dma_slave_config *cfg) {
  uint8_t channel = chan->channel;

  if ((prv_getreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel)) & DMAC_CCR_EN) != 0U) {
    return -EBUSY;
  }

  if (chan->request > MAX_REQUEST || (chan->config & ~SF32LB52_DMA_PL_MSK) != 0U) {
    return -EINVAL;
  }

  prv_putreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel),
             PBL_FIELD_PREP(DMAC_CCR_PL_MASK, chan->config & SF32LB52_DMA_PL_MSK));
  prv_putreg(ctrl, SF32LB52_DMAC_CBSR_OFFSET(channel), 0U);
  pbl_modifyl(DMAC_CSELR_CS_MASK(channel), (uint32_t)chan->request << DMAC_CSELR_CS_POS(channel),
              prv_dmac(ctrl)->base + SF32LB52_DMAC_CSELR_OFFSET(channel));

  return 0;
}

static int prv_start(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf, size_t len,
                     enum pbl_dma_dir dir, uint32_t irqs) {
  const struct pbl_dma_chan_state *state = &ctrl->chans[channel];
  uint32_t ccr = prv_getreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel));
  uint32_t width;
  uintptr_t par;

  if ((ccr & DMAC_CCR_EN) != 0U) {
    return -EBUSY;
  }

  if (dir == PBL_DMA_DEV_TO_MEM) {
    width = state->cfg.src_addr_width;
    par = state->cfg.src_addr;
  } else {
    width = state->cfg.dst_addr_width;
    par = state->cfg.dst_addr;
  }

  if ((len % width) != 0U || len / width > DMAC_CNDTR_NDT_MASK) {
    return -EINVAL;
  }

  ccr &= DMAC_CCR_PL_MASK;
  ccr |= PBL_FIELD_PREP(DMAC_CCR_PSIZE_MASK, width >> 1) |
         PBL_FIELD_PREP(DMAC_CCR_MSIZE_MASK, width >> 1) | DMAC_CCR_MINC | irqs;
  if (dir == PBL_DMA_MEM_TO_DEV) {
    ccr |= DMAC_CCR_DIR;
  }

  prv_putreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel), ccr);
  prv_putreg(ctrl, SF32LB52_DMAC_CNDTR_OFFSET(channel), len / width);
  prv_putreg(ctrl, SF32LB52_DMAC_CPAR_OFFSET(channel), par);
  prv_putreg(ctrl, SF32LB52_DMAC_CM0AR_OFFSET(channel), prv_bus_addr((uintptr_t)buf));
  prv_putreg(ctrl, SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));
  prv_putreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel), ccr | DMAC_CCR_EN);

  if (irqs != 0U) {
    pbl_irq_clear_pending(prv_dmac(ctrl)->irqs[channel]);
    pbl_irq_enable(prv_dmac(ctrl)->irqs[channel]);
  }

  return 0;
}

static int prv_prep_slave_single(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                                 size_t len, enum pbl_dma_dir dir) {
  uint32_t irqs = 0U;

  if (ctrl->chans[channel].callback != nullptr) {
    irqs = DMAC_CCR_TCIE | DMAC_CCR_TEIE;
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
                   DMAC_CCR_CIRC | DMAC_CCR_HTIE | DMAC_CCR_TCIE | DMAC_CCR_TEIE);
}

static int prv_terminate(const struct pbl_dma_controller *ctrl, uint8_t channel) {
  uint32_t ccr;

  pbl_irq_disable(prv_dmac(ctrl)->irqs[channel]);
  ccr = prv_getreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel));

  prv_putreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel),
             ccr & ~(DMAC_CCR_EN | DMAC_CCR_TCIE | DMAC_CCR_HTIE | DMAC_CCR_TEIE));
  prv_putreg(ctrl, SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));

  return 0;
}

static int prv_tx_status(const struct pbl_dma_controller *ctrl, uint8_t channel, size_t *residue) {
  uint32_t ccr = prv_getreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel));
  uint32_t count = prv_getreg(ctrl, SF32LB52_DMAC_CNDTR_OFFSET(channel)) & DMAC_CNDTR_NDT_MASK;

  *residue = (size_t)count << PBL_FIELD_GET(DMAC_CCR_PSIZE_MASK, ccr);

  return 0;
}

void pbl_dma_sf32lb52_irq_handler(const struct pbl_dma_sf32lb52_chan_irq *irq) {
  const struct pbl_dma_controller *ctrl = &irq->dmac->ctrl;
  uint8_t channel = irq->channel;
  uint32_t isr = prv_getreg(ctrl, SF32LB52_DMAC_ISR_OFFSET);

  if ((isr & DMAC_ISR_TEIF(channel)) != 0U) {
    (void)prv_terminate(ctrl, channel);
    pbl_dma_complete(ctrl, channel, -EIO);
    return;
  }

  if ((prv_getreg(ctrl, SF32LB52_DMAC_CCR_OFFSET(channel)) & DMAC_CCR_CIRC) != 0U) {
    prv_putreg(ctrl, SF32LB52_DMAC_IFCR_OFFSET, isr & DMAC_ISR_ALL(channel));
    if ((isr & DMAC_ISR_HTIF(channel)) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    if ((isr & DMAC_ISR_TCIF(channel)) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    return;
  }

  if ((isr & DMAC_ISR_TCIF(channel)) == 0U) {
    return;
  }

  prv_putreg(ctrl, SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));
  (void)prv_terminate(ctrl, channel);

  pbl_dma_complete(ctrl, channel, 0);
}

int pbl_dma_sf32lb52_init(const struct pbl_device *dev) {
  const struct pbl_dma_sf32lb52 *dmac = container_of(dev, const struct pbl_dma_sf32lb52, ctrl.dev);
  int ret = pbl_device_init(&dmac->clk.ctrl->dev);

  if (ret != 0) {
    return ret;
  }

  return pbl_clk_enable(&dmac->clk);
}

const struct pbl_dma_controller_ops pbl_dma_sf32lb52_ops = {
  .slave_config = prv_slave_config,
  .prep_slave_single = prv_prep_slave_single,
  .prep_dma_cyclic = prv_prep_dma_cyclic,
  .terminate = prv_terminate,
  .tx_status = prv_tx_status,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_dmac)
