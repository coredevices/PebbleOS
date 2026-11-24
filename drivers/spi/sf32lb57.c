/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/spi/sf32lb57.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_spi.h>

#define CLK_DIV_MIN 2U
#define CLK_DIV_MAX PBL_FIELD_GET(SPI_TOP_CTRL_CLK_DIV_MASK, UINT32_MAX)

// Frames in flight, below the 16-entry receive FIFO
#define PIO_INFLIGHT 8U

// Transmit DMA requests while the FIFO holds this many frames or fewer
#define DMA_TX_THRESHOLD 8U

// Draining the FIFO after the last DMA write, at the slowest clock
#define DRAIN_TIMEOUT_US 2000U

static const struct pbl_spi_sf32lb57 *prv_spi(const struct pbl_spi_bus *bus) {
  return PBL_CONTAINER_OF(bus, const struct pbl_spi_sf32lb57, bus);
}

static inline uint32_t prv_getreg(const struct pbl_spi_sf32lb57 *spi, uint32_t offset) {
  return pbl_readl(spi->base + offset);
}

static inline void prv_putreg(const struct pbl_spi_sf32lb57 *spi, uint32_t offset, uint32_t value) {
  pbl_writel(value, spi->base + offset);
}

// Drop what was received and clear the overrun, which transmit-only transfers leave
static void prv_rx_flush(const struct pbl_spi_sf32lb57 *spi) {
  while ((prv_getreg(spi, SF32LB57_SPI_STATUS_OFFSET) & SPI_STATUS_RNE) != 0U) {
    (void)prv_getreg(spi, SF32LB57_SPI_DATA_OFFSET);
  }

  prv_putreg(spi, SF32LB57_SPI_STATUS_OFFSET, SPI_STATUS_ROR | SPI_STATUS_TUR);
}

static int prv_init(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_sf32lb57 *spi = prv_spi(bus);
  int ret;

  ret = pbl_device_init(&spi->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  if (spi->dma_tx != nullptr) {
    const struct pbl_dma_slave_config cfg = {
      .dst_addr = spi->base + SF32LB57_SPI_DATA_OFFSET,
      .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
    };

    ret = pbl_device_init(&spi->dma_tx->ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_dma_slave_config(spi->dma_tx, &cfg);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_pinctrl_select_state(&spi->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(spi->irq);

  return 0;
}

static int prv_configure(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev) {
  const struct pbl_spi_sf32lb57 *spi = prv_spi(bus);
  uint64_t rate_mhz;
  uint32_t div;
  uint32_t top;
  int ret;

  if ((dev->mode & PBL_SPI_LSB_FIRST) != 0U || dev->max_hz == 0U) {
    return -ENOTSUP;
  }

  ret = pbl_clk_get_rate(&spi->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  div = PBL_MAX((uint32_t)PBL_DIV_ROUND_UP(rate_mhz / 1000U, dev->max_hz), CLK_DIV_MIN);
  if (div > CLK_DIV_MAX) {
    return -EINVAL;
  }

  top = SPI_TOP_CTRL_CLK_SSP_EN | PBL_FIELD_PREP(SPI_TOP_CTRL_CLK_DIV_MASK, div) |
        PBL_FIELD_PREP(SPI_TOP_CTRL_DSS_MASK, 8U - 1U);
  if ((dev->mode & PBL_SPI_CPOL) != 0U) {
    top |= SPI_TOP_CTRL_SPO;
  }
  if ((dev->mode & PBL_SPI_CPHA) != 0U) {
    top |= SPI_TOP_CTRL_SPH;
  }

  spi->state->top_ctrl = top;

  return 0;
}

static void prv_enable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_sf32lb57 *spi = prv_spi(bus);
  uint32_t top = spi->state->top_ctrl;

  pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPSLEEP);
  (void)pbl_clk_enable(&spi->clk);

  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, 0U);
  prv_putreg(spi, SF32LB57_SPI_FIFO_CTRL_OFFSET, 0U);
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL2_OFFSET, 0U);
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL_OFFSET, top);

  // The clock takes its idle level once the controller has been enabled
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL_OFFSET, top | SPI_TOP_CTRL_SSE);
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL_OFFSET, top);
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL_OFFSET, top | SPI_TOP_CTRL_SSE);

  prv_rx_flush(spi);
}

static void prv_disable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_sf32lb57 *spi = prv_spi(bus);

  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, 0U);
  prv_putreg(spi, SF32LB57_SPI_FIFO_CTRL_OFFSET, 0U);
  prv_putreg(spi, SF32LB57_SPI_TOP_CTRL_OFFSET, spi->state->top_ctrl);
  pbl_irq_clear_pending(spi->irq);

  (void)pbl_clk_disable(&spi->clk);
  pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPSLEEP);
}

// Move received frames out and queue more, keeping at most PIO_INFLIGHT in flight. Returns true
// once every frame has been received.
static bool prv_pio_service(const struct pbl_spi_sf32lb57 *spi) {
  struct pbl_spi_sf32lb57_state *state = spi->state;
  const struct pbl_spi_xfer *xfer = state->xfer;
  uint8_t *rx = xfer->rx_buf;
  const uint8_t *tx = xfer->tx_buf;

  while ((prv_getreg(spi, SF32LB57_SPI_STATUS_OFFSET) & SPI_STATUS_RNE) != 0U) {
    uint8_t byte = (uint8_t)prv_getreg(spi, SF32LB57_SPI_DATA_OFFSET);

    if (state->rx_pos < xfer->len) {
      if (rx != nullptr) {
        rx[state->rx_pos] = byte;
      }
      state->rx_pos++;
    }
  }

  while (state->tx_pos < xfer->len && state->tx_pos - state->rx_pos < PIO_INFLIGHT &&
         (prv_getreg(spi, SF32LB57_SPI_STATUS_OFFSET) & SPI_STATUS_TNF) != 0U) {
    prv_putreg(spi, SF32LB57_SPI_DATA_OFFSET, (tx != nullptr) ? tx[state->tx_pos] : 0U);
    state->tx_pos++;
  }

  return state->rx_pos == xfer->len;
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_spi_sf32lb57 *spi = arg;
  int ret;

  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, 0U);
  pbl_modifyl(SPI_FIFO_CTRL_TSRE, 0U, spi->base + SF32LB57_SPI_FIFO_CTRL_OFFSET);

  ret = PBL_POLL_BUSY_US((prv_getreg(spi, SF32LB57_SPI_STATUS_OFFSET) & SPI_STATUS_BSY) == 0U, 1U,
                         DRAIN_TIMEOUT_US);
  prv_rx_flush(spi);

  pbl_irq_clear_pending(spi->irq);
  pbl_irq_enable(spi->irq);

  pbl_spi_bus_complete(&spi->bus, (status != 0) ? status : ret);
}

// The transmit service request also needs TIE, with the interrupts masked
static int prv_start_dma(const struct pbl_spi_sf32lb57 *spi, const struct pbl_spi_xfer *xfer) {
  int ret;

  pbl_dcache_flush_range(xfer->tx_buf, xfer->len);

  pbl_irq_disable(spi->irq);
  prv_putreg(spi, SF32LB57_SPI_FIFO_CTRL_OFFSET,
             PBL_FIELD_PREP(SPI_FIFO_CTRL_TFT_MASK, DMA_TX_THRESHOLD) | SPI_FIFO_CTRL_TSRE);
  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, SPI_INTE_TIE | SPI_INTE_TIM | SPI_INTE_RIM);

  ret = pbl_dma_prep_slave_single(spi->dma_tx, (void *)xfer->tx_buf, xfer->len, PBL_DMA_MEM_TO_DEV,
                                  prv_dma_done, (void *)spi);
  if (ret != 0) {
    prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, 0U);
    pbl_irq_enable(spi->irq);
  }

  return ret;
}

static int prv_start(const struct pbl_spi_bus *bus, const struct pbl_spi_xfer *xfer) {
  const struct pbl_spi_sf32lb57 *spi = prv_spi(bus);
  struct pbl_spi_sf32lb57_state *state = spi->state;

  if (spi->dma_tx != nullptr && xfer->tx_buf != nullptr && xfer->rx_buf == nullptr &&
      xfer->len >= SPI_SF32LB57_DMA_MIN_BYTES) {
    return prv_start_dma(spi, xfer);
  }

  state->xfer = xfer;
  state->tx_pos = 0U;
  state->rx_pos = 0U;

  // Receive requests while one frame or more is waiting
  prv_putreg(spi, SF32LB57_SPI_FIFO_CTRL_OFFSET, PBL_FIELD_PREP(SPI_FIFO_CTRL_RFT_MASK, 0U));
  (void)prv_pio_service(spi);
  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, SPI_INTE_RIE);

  return 0;
}

void pbl_spi_sf32lb57_irq_handler(const struct pbl_spi_sf32lb57 *spi) {
  if ((prv_getreg(spi, SF32LB57_SPI_INTE_OFFSET) & SPI_INTE_RIE) == 0U) {
    return;
  }

  if (!prv_pio_service(spi)) {
    return;
  }

  prv_putreg(spi, SF32LB57_SPI_INTE_OFFSET, 0U);
  pbl_spi_bus_complete(&spi->bus, 0);
}

const struct pbl_spi_bus_ops pbl_spi_sf32lb57_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
};

#include PBL_DT_INSTANCES(sifli_sf32lb57_spi)
