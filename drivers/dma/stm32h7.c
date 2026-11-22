/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/dma/stm32h7.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/stm32_dma.h>
#include <hardware/stm32h7_dmamux.h>

// A stream stops once its current beat is done
#define DISABLE_TIMEOUT_US 1000U

static const struct pbl_dma_stm32h7 *prv_dmac(const struct pbl_dma_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_dma_stm32h7, ctrl);
}

static uintptr_t prv_base(const struct pbl_dma_stm32h7 *dmac, uint8_t stream) {
  return dmac->dma[stream / STM32_DMA_NUM_STREAMS];
}

static uint32_t prv_scr(uint8_t stream) {
  return STM32_DMA_SCR_OFFSET(stream % STM32_DMA_NUM_STREAMS);
}

// The flags of a stream are in the low or high register, at 0, 6, 16 or 22
static uint32_t prv_flag_shift(uint8_t stream) {
  static const uint8_t s_shifts[] = {0U, 6U, 16U, 22U};

  return s_shifts[stream % 4U];
}

static uint32_t prv_flags(const struct pbl_dma_stm32h7 *dmac, uint8_t stream) {
  uint32_t offset =
      (stream % STM32_DMA_NUM_STREAMS) < 4U ? STM32_DMA_LISR_OFFSET : STM32_DMA_HISR_OFFSET;

  return (pbl_readl(prv_base(dmac, stream) + offset) >> prv_flag_shift(stream)) & DMA_ISR_ALL;
}

static void prv_clear_flags(const struct pbl_dma_stm32h7 *dmac, uint8_t stream, uint32_t flags) {
  uint32_t offset =
      (stream % STM32_DMA_NUM_STREAMS) < 4U ? STM32_DMA_LIFCR_OFFSET : STM32_DMA_HIFCR_OFFSET;

  pbl_writel(flags << prv_flag_shift(stream), prv_base(dmac, stream) + offset);
}

// The stream a request is bound to, or -EINVAL
static int prv_stream(const struct pbl_dma_stm32h7 *dmac, uint8_t request) {
  for (uint8_t s = 0U; s < PBL_DMA_STM32H7_NUM_STREAMS; s++) {
    if (dmac->streams[s].request == request) {
      return s;
    }
  }

  return -EINVAL;
}

static int prv_slave_config(const struct pbl_dma_controller *ctrl, const struct pbl_dma_chan *chan,
                            const struct pbl_dma_slave_config *cfg) {
  const struct pbl_dma_stm32h7 *dmac = prv_dmac(ctrl);
  int stream;

  if (chan->request == 0U || chan->request != chan->channel) {
    return -EINVAL;
  }

  pbl_irq_lock();

  stream = prv_stream(dmac, chan->request);
  if (stream < 0) {
    stream = prv_stream(dmac, 0U);
  }
  if (stream < 0) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  if ((pbl_readl(prv_base(dmac, stream) + prv_scr(stream)) & DMA_SCR_EN) != 0U) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  dmac->streams[stream].request = chan->request;
  dmac->streams[stream].config = chan->config & (DMA_SCR_PL_MASK | DMA_SCR_TRBUFF);
  pbl_writel(PBL_FIELD_PREP(DMAMUX_CCR_DMAREQ_ID_MASK, chan->request),
             dmac->mux + STM32H7_DMAMUX_CCR_OFFSET(stream));

  pbl_irq_unlock();

  return 0;
}

static int prv_start(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf, size_t len,
                     enum pbl_dma_dir dir, uint32_t irqs) {
  const struct pbl_dma_stm32h7 *dmac = prv_dmac(ctrl);
  const struct pbl_dma_chan_state *state = &ctrl->chans[channel];
  int stream = prv_stream(dmac, channel);
  uintptr_t base;
  uint32_t width;
  uint32_t scr;
  uintptr_t par;

  if (stream < 0) {
    return stream;
  }

  base = prv_base(dmac, stream);
  if ((pbl_readl(base + prv_scr(stream)) & DMA_SCR_EN) != 0U) {
    return -EBUSY;
  }

  if (dir == PBL_DMA_DEV_TO_MEM) {
    width = state->cfg.src_addr_width;
    par = state->cfg.src_addr;
  } else {
    width = state->cfg.dst_addr_width;
    par = state->cfg.dst_addr;
  }

  if ((len % width) != 0U || len / width > DMA_SNDTR_MASK) {
    return -EINVAL;
  }

  scr = dmac->streams[stream].config | DMA_SCR_MINC |
        PBL_FIELD_PREP(DMA_SCR_PSIZE_MASK, width >> 1) |
        PBL_FIELD_PREP(DMA_SCR_MSIZE_MASK, width >> 1) |
        PBL_FIELD_PREP(DMA_SCR_DIR_MASK,
                       dir == PBL_DMA_MEM_TO_DEV ? DMA_SCR_DIR_M2P : DMA_SCR_DIR_P2M) |
        irqs;

  prv_clear_flags(dmac, stream, DMA_ISR_ALL);
  pbl_writel(scr, base + prv_scr(stream));
  pbl_writel(len / width, base + STM32_DMA_SNDTR_OFFSET(stream % STM32_DMA_NUM_STREAMS));
  pbl_writel(par, base + STM32_DMA_SPAR_OFFSET(stream % STM32_DMA_NUM_STREAMS));
  pbl_writel((uintptr_t)buf, base + STM32_DMA_SM0AR_OFFSET(stream % STM32_DMA_NUM_STREAMS));

  if (irqs != 0U) {
    pbl_irq_clear_pending(dmac->irqs[stream]);
    pbl_irq_enable(dmac->irqs[stream]);
  }

  pbl_writel(scr | DMA_SCR_EN, base + prv_scr(stream));

  return 0;
}

static int prv_prep_slave_single(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                                 size_t len, enum pbl_dma_dir dir) {
  uint32_t irqs = 0U;

  if (ctrl->chans[channel].callback != nullptr) {
    irqs = DMA_SCR_TCIE | DMA_SCR_TEIE;
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
                   DMA_SCR_CIRC | DMA_SCR_HTIE | DMA_SCR_TCIE | DMA_SCR_TEIE);
}

static void prv_stop(const struct pbl_dma_stm32h7 *dmac, uint8_t stream) {
  uintptr_t scr = prv_base(dmac, stream) + prv_scr(stream);

  pbl_irq_disable(dmac->irqs[stream]);
  pbl_modifyl(DMA_SCR_EN | DMA_SCR_TCIE | DMA_SCR_HTIE | DMA_SCR_TEIE | DMA_SCR_DMEIE, 0U, scr);
  (void)PBL_POLL_BUSY_US((pbl_readl(scr) & DMA_SCR_EN) == 0U, 1U, DISABLE_TIMEOUT_US);
  prv_clear_flags(dmac, stream, DMA_ISR_ALL);
}

static int prv_terminate(const struct pbl_dma_controller *ctrl, uint8_t channel) {
  const struct pbl_dma_stm32h7 *dmac = prv_dmac(ctrl);
  int stream = prv_stream(dmac, channel);

  if (stream < 0) {
    return stream;
  }

  prv_stop(dmac, stream);

  return 0;
}

static int prv_tx_status(const struct pbl_dma_controller *ctrl, uint8_t channel, size_t *residue) {
  const struct pbl_dma_stm32h7 *dmac = prv_dmac(ctrl);
  int stream = prv_stream(dmac, channel);
  uintptr_t base;
  uint32_t scr;

  if (stream < 0) {
    return stream;
  }

  base = prv_base(dmac, stream);
  scr = pbl_readl(base + prv_scr(stream));
  *residue = (size_t)(pbl_readl(base + STM32_DMA_SNDTR_OFFSET(stream % STM32_DMA_NUM_STREAMS)) &
                      DMA_SNDTR_MASK)
             << PBL_FIELD_GET(DMA_SCR_PSIZE_MASK, scr);

  return 0;
}

void pbl_dma_stm32h7_irq_handler(const struct pbl_dma_stm32h7_stream_irq *irq) {
  const struct pbl_dma_stm32h7 *dmac = irq->dmac;
  const struct pbl_dma_controller *ctrl = &dmac->ctrl;
  uint8_t stream = irq->stream;
  uint8_t channel = dmac->streams[stream].request;
  uint32_t flags = prv_flags(dmac, stream);

  if ((flags & DMA_ISR_TEIF) != 0U) {
    prv_stop(dmac, stream);
    pbl_dma_complete(ctrl, channel, -EIO);
    return;
  }

  prv_clear_flags(dmac, stream, flags);

  if ((pbl_readl(prv_base(dmac, stream) + prv_scr(stream)) & DMA_SCR_CIRC) != 0U) {
    if ((flags & DMA_ISR_HTIF) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    if ((flags & DMA_ISR_TCIF) != 0U) {
      pbl_dma_complete(ctrl, channel, 0);
    }
    return;
  }

  if ((flags & DMA_ISR_TCIF) == 0U) {
    return;
  }

  prv_stop(dmac, stream);
  pbl_dma_complete(ctrl, channel, 0);
}

int pbl_dma_stm32h7_init(const struct pbl_device *dev) {
  const struct pbl_dma_stm32h7 *dmac =
      PBL_CONTAINER_OF(dev, const struct pbl_dma_stm32h7, ctrl.dev);
  int ret;

  for (uint32_t i = 0U; i < PBL_ARRAY_SIZE(dmac->clk); i++) {
    ret = pbl_device_init(&dmac->clk[i].ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_clk_enable(&dmac->clk[i]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

const struct pbl_dma_controller_ops pbl_dma_stm32h7_ops = {
  .slave_config = prv_slave_config,
  .prep_slave_single = prv_prep_slave_single,
  .prep_dma_cyclic = prv_prep_dma_cyclic,
  .terminate = prv_terminate,
  .tx_status = prv_tx_status,
};

#include PBL_DT_INSTANCES(st_stm32h7_dmamux_dma_st_stm32_dma)
