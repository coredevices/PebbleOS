/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/uart/stm32.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_usart.h>

#define ICR_ERRORS (USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF)

static const struct pbl_uart_stm32 *prv_stm32(const struct pbl_uart *uart) {
  return PBL_CONTAINER_OF(uart, const struct pbl_uart_stm32, uart);
}

static inline uintptr_t prv_reg(const struct pbl_uart_stm32 *usart, uint32_t offset) {
  return usart->base + offset;
}

static inline uint32_t prv_getreg(const struct pbl_uart_stm32 *usart, uint32_t offset) {
  return pbl_readl(prv_reg(usart, offset));
}

static inline void prv_putreg(const struct pbl_uart_stm32 *usart, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_reg(usart, offset));
}

static void prv_flush(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  if (!usart->state->tx_pending) {
    return;
  }

  while ((prv_getreg(usart, STM32H7_USART_ISR_OFFSET) & USART_ISR_TC) == 0U) {
  }
  usart->state->tx_pending = false;
}

static int prv_early_init(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);
  int ret;

  ret = pbl_device_init(&usart->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&usart->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&usart->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  prv_flush(uart);

  prv_putreg(usart, STM32H7_USART_CR1_OFFSET, 0U);
  prv_putreg(usart, STM32H7_USART_CR2_OFFSET, 0U);
  prv_putreg(usart, STM32H7_USART_CR3_OFFSET, 0U);
  prv_putreg(usart, STM32H7_USART_PRESC_OFFSET, 0U);

  return 0;
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);
  int ret;

  ret = prv_early_init(uart);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(usart->irq);

  return 0;
}

// Frame settings need the USART disabled
static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);
  uint32_t cr1 = 0U;
  uint32_t cr3 = 0U;
  uint64_t rate_mhz;
  uint32_t clk_hz;
  uint32_t brr;
  int ret;

  ret = pbl_clk_get_rate(&usart->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  clk_hz = (uint32_t)(rate_mhz / 1000U);
  brr = (clk_hz + cfg->baudrate / 2U) / cfg->baudrate;
  if (brr < USART_BRR_MIN || brr > USART_BRR_MAX) {
    return -ENOTSUP;
  }

  // Parity takes the ninth bit of the word
  switch (cfg->parity) {
    case PBL_UART_PARITY_NONE:
      break;
    case PBL_UART_PARITY_ODD:
      cr1 = USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS;
      break;
    case PBL_UART_PARITY_EVEN:
      cr1 = USART_CR1_M0 | USART_CR1_PCE;
      break;
    default:
      return -ENOTSUP;
  }

  switch (cfg->flow_ctrl) {
    case PBL_UART_FLOW_CTRL_NONE:
      break;
    case PBL_UART_FLOW_CTRL_RTS_CTS:
      cr3 = USART_CR3_RTSE | USART_CR3_CTSE;
      break;
    default:
      return -ENOTSUP;
  }

  prv_flush(uart);

  pbl_modifyl(USART_CR1_UE, 0U, prv_reg(usart, STM32H7_USART_CR1_OFFSET));
  pbl_modifyl(USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS, cr1,
              prv_reg(usart, STM32H7_USART_CR1_OFFSET));
  pbl_modifyl(USART_CR3_RTSE | USART_CR3_CTSE, cr3, prv_reg(usart, STM32H7_USART_CR3_OFFSET));
  prv_putreg(usart, STM32H7_USART_BRR_OFFSET, brr);
  pbl_modifyl(0U, USART_CR1_TE | USART_CR1_RE | USART_CR1_UE,
              prv_reg(usart, STM32H7_USART_CR1_OFFSET));

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  while ((prv_getreg(usart, STM32H7_USART_ISR_OFFSET) & USART_ISR_TXE) == 0U) {
  }
  prv_putreg(usart, STM32H7_USART_TDR_OFFSET, c);
  usart->state->tx_pending = true;
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  prv_putreg(usart, STM32H7_USART_RQR_OFFSET, USART_RQR_RXFRQ);
  prv_putreg(usart, STM32H7_USART_ICR_OFFSET, ICR_ERRORS);
  pbl_modifyl(0U, USART_CR1_RXNEIE, prv_reg(usart, STM32H7_USART_CR1_OFFSET));

  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  pbl_modifyl(USART_CR1_RXNEIE, 0U, prv_reg(usart, STM32H7_USART_CR1_OFFSET));
}

void pbl_uart_stm32_irq_handler(const struct pbl_uart_stm32 *usart) {
  uint32_t isr = prv_getreg(usart, STM32H7_USART_ISR_OFFSET);

  if ((prv_getreg(usart, STM32H7_USART_CR1_OFFSET) & USART_CR1_RXNEIE) == 0U) {
    return;
  }

  // An overrun keeps the interrupt pending until cleared
  if ((isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) != 0U) {
    prv_putreg(usart, STM32H7_USART_ICR_OFFSET, ICR_ERRORS);
  }

  if ((isr & USART_ISR_RXNE) != 0U) {
    (void)pbl_uart_receive(&usart->uart, (uint8_t)prv_getreg(usart, STM32H7_USART_RDR_OFFSET));
  }
}

const struct pbl_uart_ops pbl_uart_stm32_ops = {
  .init = prv_init,
  .early_init = prv_early_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .flush = prv_flush,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

#include PBL_DT_INSTANCES(st_stm32h7_uart)
