/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <hardware/sf32lb52_dmac.h>

#define MAX_REQUEST 0x3fU

// The MPI (XIP flash) code bus window, which the DMAC reaches through the system bus instead
#define MPI_CBUS_START  0x10000000U
#define MPI_CBUS_END    0x20000000U
#define MPI_CBUS_2_SBUS 0x50000000U

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_DMAC1);

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_DMAC1_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_DMAC1_BASE + offset);
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

  if ((prv_getreg(SF32LB52_DMAC_CCR_OFFSET(channel)) & DMAC_CCR_EN) != 0U) {
    return -EBUSY;
  }

  if (chan->request > MAX_REQUEST || (chan->config & ~SF32LB52_DMA_PL_MSK) != 0U) {
    return -EINVAL;
  }

  prv_putreg(SF32LB52_DMAC_CCR_OFFSET(channel),
             PBL_FIELD_PREP(DMAC_CCR_PL_MASK, chan->config & SF32LB52_DMA_PL_MSK));
  prv_putreg(SF32LB52_DMAC_CBSR_OFFSET(channel), 0U);
  pbl_modifyl(DMAC_CSELR_CS_MASK(channel), (uint32_t)chan->request << DMAC_CSELR_CS_POS(channel),
              SF32LB52_DMAC1_BASE + SF32LB52_DMAC_CSELR_OFFSET(channel));

  return 0;
}

static int prv_prep_slave_single(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                                 size_t len, enum pbl_dma_dir dir) {
  const struct pbl_dma_chan_state *state = &ctrl->chans[channel];
  uint32_t ccr = prv_getreg(SF32LB52_DMAC_CCR_OFFSET(channel));
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
         PBL_FIELD_PREP(DMAC_CCR_MSIZE_MASK, width >> 1) | DMAC_CCR_MINC;
  if (dir == PBL_DMA_MEM_TO_DEV) {
    ccr |= DMAC_CCR_DIR;
  }
  if (state->callback != nullptr) {
    ccr |= DMAC_CCR_TCIE | DMAC_CCR_TEIE;
  }

  prv_putreg(SF32LB52_DMAC_CCR_OFFSET(channel), ccr);
  prv_putreg(SF32LB52_DMAC_CNDTR_OFFSET(channel), len / width);
  prv_putreg(SF32LB52_DMAC_CPAR_OFFSET(channel), par);
  prv_putreg(SF32LB52_DMAC_CM0AR_OFFSET(channel), prv_bus_addr((uintptr_t)buf));
  prv_putreg(SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));
  prv_putreg(SF32LB52_DMAC_CCR_OFFSET(channel), ccr | DMAC_CCR_EN);

  return 0;
}

static int prv_terminate(const struct pbl_dma_controller *ctrl, uint8_t channel) {
  uint32_t ccr = prv_getreg(SF32LB52_DMAC_CCR_OFFSET(channel));

  prv_putreg(SF32LB52_DMAC_CCR_OFFSET(channel),
             ccr & ~(DMAC_CCR_EN | DMAC_CCR_TCIE | DMAC_CCR_HTIE | DMAC_CCR_TEIE));
  prv_putreg(SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));

  return 0;
}

static int prv_tx_status(const struct pbl_dma_controller *ctrl, uint8_t channel, size_t *residue) {
  uint32_t ccr = prv_getreg(SF32LB52_DMAC_CCR_OFFSET(channel));
  uint32_t count = prv_getreg(SF32LB52_DMAC_CNDTR_OFFSET(channel)) & DMAC_CNDTR_NDT_MASK;

  *residue = (size_t)count << PBL_FIELD_GET(DMAC_CCR_PSIZE_MASK, ccr);

  return 0;
}

void pbl_dma_sf32lb52_irq_handler(const struct pbl_dma_chan *chan) {
  uint8_t channel = chan->channel;
  uint32_t isr = prv_getreg(SF32LB52_DMAC_ISR_OFFSET);
  int status;

  if ((isr & DMAC_ISR_TEIF(channel)) != 0U) {
    status = -EIO;
  } else if ((isr & DMAC_ISR_TCIF(channel)) != 0U) {
    status = 0;
  } else {
    return;
  }

  prv_putreg(SF32LB52_DMAC_IFCR_OFFSET, DMAC_ISR_ALL(channel));
  (void)prv_terminate(chan->ctrl, channel);

  pbl_dma_complete(chan->ctrl, channel, status);
}

static int prv_init(const struct pbl_device *dev) {
  int ret = pbl_device_init(&s_clk.ctrl->dev);

  if (ret != 0) {
    return ret;
  }

  return pbl_clk_enable(&s_clk);
}

static const struct pbl_dma_controller_ops s_ops = {
  .slave_config = prv_slave_config,
  .prep_slave_single = prv_prep_slave_single,
  .terminate = prv_terminate,
  .tx_status = prv_tx_status,
};

PBL_DMA_CONTROLLER_STATE_DEFINE(pbl_dma_sf32lb52_dmac1, SF32LB52_DMAC_NUM_CHANNELS);
const struct pbl_dma_controller pbl_dma_sf32lb52_dmac1 =
    PBL_DMA_CONTROLLER_INIT(pbl_dma_sf32lb52_dmac1, "dmac1", prv_init, &s_ops);
PBL_DEVICE_REGISTER(pbl_dma_sf32lb52_dmac1, &pbl_dma_sf32lb52_dmac1.dev);
