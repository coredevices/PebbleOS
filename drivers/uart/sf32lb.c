/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_usart.h>

// The USART kernel clock is always 48 MHz on the SF32LB52
#define USART_CLK_HZ 48000000U

#define ICR_ERRORS (UART_ICR_PECF | UART_ICR_FECF | UART_ICR_NCF | UART_ICR_ORECF)

static const struct {
  uintptr_t base;
  struct pbl_clk clk;
  pbl_irq_t irqn;
} s_instances[] = {
  {SF32LB52_USART1_BASE, PBL_CLK(nullptr, 0), PBL_IRQN(USART1)},
  {SF32LB52_USART2_BASE, PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_USART2), PBL_IRQN(USART2)},
  {SF32LB52_USART3_BASE, PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_USART3), PBL_IRQN(USART3)},
};

static const struct pbl_uart_sf32lb *prv_sf32lb(const struct pbl_uart *uart) {
  return container_of(uart, const struct pbl_uart_sf32lb, uart);
}

static inline uintptr_t prv_reg(const struct pbl_uart_sf32lb *usart, uint32_t offset) {
  return s_instances[usart->index - 1U].base + offset;
}

static inline uint32_t prv_getreg(const struct pbl_uart_sf32lb *usart, uint32_t offset) {
  return pbl_readl(prv_reg(usart, offset));
}

static inline void prv_putreg(const struct pbl_uart_sf32lb *usart, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, prv_reg(usart, offset));
}

// TC reads clear after power-on until a byte has been sent
static void prv_flush(const struct pbl_uart *uart) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);

  if (!usart->state->tx_pending) {
    return;
  }

  while ((prv_getreg(usart, SF32LB52_USART_ISR_OFFSET) & UART_ISR_TC) == 0U) {
  }
  usart->state->tx_pending = false;
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);
  const struct pbl_clk *clk;
  int ret;

  if (usart->index == 0U || usart->index > ARRAY_LENGTH(s_instances) ||
      (usart->rx_dma != nullptr) != (usart->rx_buf != nullptr)) {
    return -EINVAL;
  }

  clk = &s_instances[usart->index - 1U].clk;
  if (clk->ctrl != nullptr) {
    ret = pbl_device_init(&clk->ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_clk_enable(clk);
    if (ret != 0) {
      return ret;
    }
  }

  if (usart->rx_dma != nullptr) {
    const struct pbl_dma_slave_config cfg = {
      .src_addr = prv_reg(usart, SF32LB52_USART_RDR_OFFSET),
      .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
    };

    ret = pbl_device_init(&usart->rx_dma->ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_dma_slave_config(usart->rx_dma, &cfg);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_pinctrl_select_state(&usart->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  prv_flush(uart);

  prv_putreg(usart, SF32LB52_USART_CR1_OFFSET, 0U);
  prv_putreg(usart, SF32LB52_USART_CR2_OFFSET, 0U);
  prv_putreg(usart, SF32LB52_USART_CR3_OFFSET, 0U);
  prv_putreg(usart, SF32LB52_USART_CR1_OFFSET, UART_CR1_TE | UART_CR1_UE);

  pbl_irq_enable(s_instances[usart->index - 1U].irqn);

  return 0;
}

// Frame settings need the USART disabled, the baud rate only takes once it is enabled
static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);
  uint32_t brr = USART_CLK_HZ / cfg->baudrate;
  uint32_t cr1 = PBL_FIELD_PREP(UART_CR1_M_MASK, UART_CR1_M_8BIT);
  uint32_t cr3 = 0U;
  uint32_t smplini = UART_MISCR_SMPLINI_16X;

  switch (cfg->parity) {
    case PBL_UART_PARITY_NONE:
      break;
    case PBL_UART_PARITY_ODD:
      cr1 = PBL_FIELD_PREP(UART_CR1_M_MASK, UART_CR1_M_9BIT) | UART_CR1_PCE | UART_CR1_PS;
      break;
    case PBL_UART_PARITY_EVEN:
      cr1 = PBL_FIELD_PREP(UART_CR1_M_MASK, UART_CR1_M_9BIT) | UART_CR1_PCE;
      break;
    default:
      return -ENOTSUP;
  }

  switch (cfg->flow_ctrl) {
    case PBL_UART_FLOW_CTRL_NONE:
      break;
    case PBL_UART_FLOW_CTRL_RTS_CTS:
      cr3 = UART_CR3_RTSE | UART_CR3_CTSE;
      break;
    default:
      return -ENOTSUP;
  }

  if (brr < USART_BRR_MIN) {
    brr = (2U * USART_CLK_HZ) / cfg->baudrate;
    cr1 |= UART_CR1_OVER8;
    smplini = UART_MISCR_SMPLINI_8X;
  }

  if (brr < USART_BRR_MIN) {
    return -ENOTSUP;
  }

  prv_flush(uart);

  pbl_modifyl(UART_CR1_UE, 0U, prv_reg(usart, SF32LB52_USART_CR1_OFFSET));
  pbl_modifyl(UART_CR1_M_MASK | UART_CR1_PCE | UART_CR1_PS | UART_CR1_OVER8, cr1,
              prv_reg(usart, SF32LB52_USART_CR1_OFFSET));
  pbl_modifyl(UART_CR3_RTSE | UART_CR3_CTSE, cr3, prv_reg(usart, SF32LB52_USART_CR3_OFFSET));
  pbl_modifyl(0U, UART_CR1_UE, prv_reg(usart, SF32LB52_USART_CR1_OFFSET));

  pbl_modifyl(UART_MISCR_SMPLINI_MASK, PBL_FIELD_PREP(UART_MISCR_SMPLINI_MASK, smplini),
              prv_reg(usart, SF32LB52_USART_MISCR_OFFSET));
  prv_putreg(usart, SF32LB52_USART_BRR_OFFSET, brr);

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);

  while ((prv_getreg(usart, SF32LB52_USART_ISR_OFFSET) & UART_ISR_TXE) == 0U) {
  }
  prv_putreg(usart, SF32LB52_USART_TDR_OFFSET, c);
  usart->state->tx_pending = true;
}

static void prv_dma_drain(const struct pbl_uart_sf32lb *usart) {
  struct pbl_uart_sf32lb_state *state = usart->state;
  size_t residue;
  uint32_t head;

  pbl_irq_lock();

  if (!usart->uart.state->rx_enabled || pbl_dma_tx_status(usart->rx_dma, &residue) != 0) {
    pbl_irq_unlock();
    return;
  }

  head = (PBL_UART_SF32LB_RX_BUF_SIZE - residue) % PBL_UART_SF32LB_RX_BUF_SIZE;
  dcache_invalidate(usart->rx_buf, PBL_UART_SF32LB_RX_BUF_SIZE);

  while (state->rx_pos != head) {
    uint8_t c = usart->rx_buf[state->rx_pos];

    state->rx_pos = (state->rx_pos + 1U) % PBL_UART_SF32LB_RX_BUF_SIZE;
    if (!pbl_uart_receive(&usart->uart, c)) {
      break;
    }
  }

  pbl_irq_unlock();
}

static void prv_dma_callback(void *arg, int status) {
  prv_dma_drain(arg);
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);
  uint32_t cr1 = UART_CR1_RE | UART_CR1_RXNEIE;
  int ret;

  (void)prv_getreg(usart, SF32LB52_USART_RDR_OFFSET);
  prv_putreg(usart, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS | UART_ICR_IDLECF);

  if (usart->rx_dma != nullptr) {
    // The buffer is invalidated whole, so it must not share cache lines with anything else
    dcache_flush_invalidate(usart->rx_buf, PBL_UART_SF32LB_RX_BUF_SIZE);
    usart->state->rx_pos = 0U;

    ret = pbl_dma_prep_dma_cyclic(usart->rx_dma, usart->rx_buf, PBL_UART_SF32LB_RX_BUF_SIZE,
                                  PBL_UART_SF32LB_RX_BUF_SIZE / 2U, PBL_DMA_DEV_TO_MEM,
                                  prv_dma_callback, (void *)usart);
    if (ret != 0) {
      return ret;
    }

    pbl_modifyl(0U, UART_CR3_DMAR, prv_reg(usart, SF32LB52_USART_CR3_OFFSET));
    cr1 = UART_CR1_RE | UART_CR1_IDLEIE;
  }

  pbl_modifyl(0U, cr1, prv_reg(usart, SF32LB52_USART_CR1_OFFSET));

  // The USART does not receive in deep sleep
  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP);

  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
  const struct pbl_uart_sf32lb *usart = prv_sf32lb(uart);

  pbl_modifyl(UART_CR1_RE | UART_CR1_RXNEIE | UART_CR1_IDLEIE, 0U,
              prv_reg(usart, SF32LB52_USART_CR1_OFFSET));

  if (usart->rx_dma != nullptr) {
    (void)pbl_dma_terminate(usart->rx_dma);
    pbl_modifyl(UART_CR3_DMAR, 0U, prv_reg(usart, SF32LB52_USART_CR3_OFFSET));
  }

  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
}

void pbl_uart_sf32lb_irq_handler(const struct pbl_uart_sf32lb *usart) {
  uint32_t isr = prv_getreg(usart, SF32LB52_USART_ISR_OFFSET);
  uint32_t cr1 = prv_getreg(usart, SF32LB52_USART_CR1_OFFSET);

  if ((cr1 & UART_CR1_IDLEIE) != 0U && (isr & UART_ISR_IDLE) != 0U) {
    prv_putreg(usart, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS | UART_ICR_IDLECF);
    prv_dma_drain(usart);
  }

  if ((cr1 & UART_CR1_RXNEIE) != 0U && (isr & (UART_ISR_RXNE | UART_ISR_ORE)) != 0U) {
    uint8_t c = PBL_FIELD_GET(USART_RDR_MASK, prv_getreg(usart, SF32LB52_USART_RDR_OFFSET));

    prv_putreg(usart, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS);
    if ((isr & UART_ISR_RXNE) != 0U) {
      (void)pbl_uart_receive(&usart->uart, c);
    }
  }
}

const struct pbl_uart_ops pbl_uart_sf32lb_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .flush = prv_flush,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};
