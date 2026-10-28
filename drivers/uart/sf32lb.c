/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
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

static inline uintptr_t prv_reg(UARTDevice *dev, uint32_t offset) {
  return s_instances[dev->index - 1U].base + offset;
}

static inline uint32_t prv_getreg(UARTDevice *dev, uint32_t offset) {
  return pbl_readl(prv_reg(dev, offset));
}

static inline void prv_putreg(UARTDevice *dev, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_reg(dev, offset));
}

void uart_init(UARTDevice *dev) {
  const struct pbl_clk *clk;

  PBL_ASSERTN(dev->index >= 1U && dev->index <= ARRAY_LENGTH(s_instances));

  clk = &s_instances[dev->index - 1U].clk;
  if (clk->ctrl != nullptr) {
    PBL_ASSERTN(pbl_device_init(&clk->ctrl->dev) == 0);
    PBL_ASSERTN(pbl_clk_enable(clk) == 0);
  }

  if (dev->rx_dma != nullptr) {
    const struct pbl_dma_slave_config cfg = {
      .src_addr = prv_reg(dev, SF32LB52_USART_RDR_OFFSET),
      .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
    };

    PBL_ASSERTN(pbl_device_init(&dev->rx_dma->ctrl->dev) == 0);
    PBL_ASSERTN(pbl_dma_slave_config(dev->rx_dma, &cfg) == 0);
  }

  PBL_ASSERTN(pbl_pinctrl_select_state(&dev->pinctrl, PBL_PINCTRL_STATE_DEFAULT) == 0);

  prv_putreg(dev, SF32LB52_USART_CR1_OFFSET, 0U);
  prv_putreg(dev, SF32LB52_USART_CR2_OFFSET, 0U);
  prv_putreg(dev, SF32LB52_USART_CR3_OFFSET, 0U);
  prv_putreg(dev, SF32LB52_USART_CR1_OFFSET,
             PBL_FIELD_PREP(UART_CR1_M_MASK, UART_CR1_M_8BIT) | UART_CR1_TE | UART_CR1_UE);

  pbl_irq_enable(s_instances[dev->index - 1U].irqn);
}

void uart_init_tx_only(UARTDevice *dev) {
  uart_init(dev);
}

// The baud rate only takes once the USART is enabled
void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {
  uint32_t brr = USART_CLK_HZ / baud_rate;
  uint32_t smplini = UART_MISCR_SMPLINI_16X;

  uart_wait_for_tx_complete(dev);

  if (brr < USART_BRR_MIN) {
    brr = (2U * USART_CLK_HZ) / baud_rate;
    smplini = UART_MISCR_SMPLINI_8X;
    pbl_modifyl(UART_CR1_OVER8, UART_CR1_OVER8, prv_reg(dev, SF32LB52_USART_CR1_OFFSET));
  } else {
    pbl_modifyl(UART_CR1_OVER8, 0U, prv_reg(dev, SF32LB52_USART_CR1_OFFSET));
  }

  pbl_modifyl(UART_MISCR_SMPLINI_MASK, PBL_FIELD_PREP(UART_MISCR_SMPLINI_MASK, smplini),
              prv_reg(dev, SF32LB52_USART_MISCR_OFFSET));
  prv_putreg(dev, SF32LB52_USART_BRR_OFFSET, brr);
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  pbl_irq_lock();
  while ((prv_getreg(dev, SF32LB52_USART_ISR_OFFSET) & UART_ISR_TXE) == 0U) {
  }
  prv_putreg(dev, SF32LB52_USART_TDR_OFFSET, data);
  dev->state->tx_pending = true;
  pbl_irq_unlock();
}

// TC reads clear after power-on until a byte has been sent
void uart_wait_for_tx_complete(UARTDevice *dev) {
  if (!dev->state->tx_pending) {
    return;
  }

  while ((prv_getreg(dev, SF32LB52_USART_ISR_OFFSET) & UART_ISR_TC) == 0U) {
  }
  dev->state->tx_pending = false;
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  dev->state->rx_irq_handler = irq_handler;
}

static void prv_rx_deliver(UARTDevice *dev, uint8_t c, const UARTRXErrorFlags *err_flags) {
  if (dev->state->rx_irq_handler != nullptr) {
    dev->state->rx_irq_handler(dev, c, err_flags);
  }
}

static void prv_dma_drain(UARTDevice *dev) {
  UARTDeviceState *state = dev->state;
  const UARTRXErrorFlags err_flags = {};
  size_t residue;
  uint32_t head;

  pbl_irq_lock();

  if (state->rx_buf == nullptr || pbl_dma_tx_status(dev->rx_dma, &residue) != 0) {
    pbl_irq_unlock();
    return;
  }

  head = (state->rx_len - residue) % state->rx_len;
  dcache_invalidate(state->rx_buf, state->rx_len);

  while (state->rx_int_enabled && state->rx_pos != head) {
    uint8_t c = state->rx_buf[state->rx_pos];

    state->rx_pos = (state->rx_pos + 1U) % state->rx_len;
    prv_rx_deliver(dev, c, &err_flags);
  }

  pbl_irq_unlock();
}

static void prv_dma_callback(void *arg, int status) {
  prv_dma_drain(arg);
}

static void prv_rx_stop(UARTDevice *dev) {
  pbl_modifyl(UART_CR1_RE | UART_CR1_RXNEIE | UART_CR1_IDLEIE, 0U,
              prv_reg(dev, SF32LB52_USART_CR1_OFFSET));

  if ((prv_getreg(dev, SF32LB52_USART_CR3_OFFSET) & UART_CR3_DMAR) != 0U) {
    (void)pbl_dma_terminate(dev->rx_dma);
    pbl_modifyl(UART_CR3_DMAR, 0U, prv_reg(dev, SF32LB52_USART_CR3_OFFSET));
  }
}

static void prv_rx_start(UARTDevice *dev) {
  UARTDeviceState *state = dev->state;

  (void)prv_getreg(dev, SF32LB52_USART_RDR_OFFSET);
  prv_putreg(dev, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS | UART_ICR_IDLECF);

  if (state->rx_buf != nullptr) {
    state->rx_pos = 0U;
    pbl_modifyl(UART_CR3_DMAR, UART_CR3_DMAR, prv_reg(dev, SF32LB52_USART_CR3_OFFSET));
    PBL_ASSERTN(pbl_dma_prep_dma_cyclic(dev->rx_dma, state->rx_buf, state->rx_len,
                                        state->rx_len / 2U, PBL_DMA_DEV_TO_MEM, prv_dma_callback,
                                        (void *)dev) == 0);
    pbl_modifyl(UART_CR1_RE | UART_CR1_IDLEIE, UART_CR1_RE | UART_CR1_IDLEIE,
                prv_reg(dev, SF32LB52_USART_CR1_OFFSET));
  } else {
    pbl_modifyl(UART_CR1_RE | UART_CR1_RXNEIE, UART_CR1_RE | UART_CR1_RXNEIE,
                prv_reg(dev, SF32LB52_USART_CR1_OFFSET));
  }
}

static void prv_rx_restart(UARTDevice *dev) {
  pbl_irq_lock();
  prv_rx_stop(dev);
  if (dev->state->rx_int_enabled) {
    prv_rx_start(dev);
  }
  pbl_irq_unlock();
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  UARTDeviceState *state = dev->state;
  bool was_enabled = state->rx_int_enabled;

  state->rx_int_enabled = enabled;
  prv_rx_restart(dev);

  // The USART does not receive in deep sleep
  if (enabled && !was_enabled) {
    soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP);
  } else if (!enabled && was_enabled) {
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
  }
}

void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length) {
  const uintptr_t line_mask = dcache_line_size() - 1U;

  // The buffer is invalidated whole, so it must not share cache lines with anything else
  PBL_ASSERTN(dev->rx_dma != nullptr && ((uintptr_t)buffer & line_mask) == 0U &&
              (length & line_mask) == 0U);
  dcache_flush_invalidate(buffer, length);

  dev->state->rx_buf = buffer;
  dev->state->rx_len = length;
  prv_rx_restart(dev);
}

void uart_stop_rx_dma(UARTDevice *dev) {
  dev->state->rx_buf = nullptr;
  prv_rx_restart(dev);
}

void uart_irq_handler(UARTDevice *dev) {
  uint32_t isr = prv_getreg(dev, SF32LB52_USART_ISR_OFFSET);
  uint32_t cr1 = prv_getreg(dev, SF32LB52_USART_CR1_OFFSET);

  if ((cr1 & UART_CR1_IDLEIE) != 0U && (isr & UART_ISR_IDLE) != 0U) {
    prv_putreg(dev, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS | UART_ICR_IDLECF);
    prv_dma_drain(dev);
  }

  if ((cr1 & UART_CR1_RXNEIE) != 0U && (isr & (UART_ISR_RXNE | UART_ISR_ORE)) != 0U) {
    const UARTRXErrorFlags err_flags = {
      .overrun_error = (isr & UART_ISR_ORE) != 0U,
      .framing_error = (isr & UART_ISR_FE) != 0U,
    };
    uint8_t c = PBL_FIELD_GET(USART_RDR_MASK, prv_getreg(dev, SF32LB52_USART_RDR_OFFSET));

    prv_putreg(dev, SF32LB52_USART_ICR_OFFSET, ICR_ERRORS);
    if ((isr & UART_ISR_RXNE) != 0U) {
      prv_rx_deliver(dev, c, &err_flags);
    }
  }
}
