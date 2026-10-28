/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/uart/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

// QEMU pebble-simple-uart registers
#define UART_DATA_OFFSET  0x00
#define UART_STATE_OFFSET 0x04
#define UART_CTRL_OFFSET  0x08
#define UART_INT_OFFSET   0x0c

#define STATE_TX_READY PBL_BIT(0)
#define STATE_RX_READY PBL_BIT(1)

#define CTRL_RX_IE PBL_BIT(1)

#define INT_TX_PENDING PBL_BIT(0)
#define INT_RX_PENDING PBL_BIT(1)

#define DATA_MASK PBL_GENMASK(7, 0)

static const struct pbl_uart_qemu *prv_qemu(const struct pbl_uart *uart) {
  return container_of(uart, const struct pbl_uart_qemu, uart);
}

static inline uint32_t prv_getreg(const struct pbl_uart_qemu *qemu, uint32_t offset) {
  return pbl_readl(qemu->base + offset);
}

static inline void prv_putreg(const struct pbl_uart_qemu *qemu, uint32_t offset, uint32_t value) {
  pbl_writel(value, qemu->base + offset);
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_qemu *qemu = prv_qemu(uart);

  prv_putreg(qemu, UART_CTRL_OFFSET, 0U);
  prv_putreg(qemu, UART_INT_OFFSET, INT_RX_PENDING | INT_TX_PENDING);
  pbl_irq_enable(qemu->irqn);

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  if (cfg->parity != PBL_UART_PARITY_NONE || cfg->flow_ctrl != PBL_UART_FLOW_CTRL_NONE) {
    return -ENOTSUP;
  }

  return 0;
}

static void prv_flush(const struct pbl_uart *uart) {
  while ((prv_getreg(prv_qemu(uart), UART_STATE_OFFSET) & STATE_TX_READY) == 0U) {
  }
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  prv_flush(uart);
  prv_putreg(prv_qemu(uart), UART_DATA_OFFSET, c);
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  pbl_modifyl(0U, CTRL_RX_IE, prv_qemu(uart)->base + UART_CTRL_OFFSET);

  return 0;
}

// The receive interrupt is level-sensitive, clear it before reception is marked disabled
static void prv_rx_disable(const struct pbl_uart *uart) {
  pbl_modifyl(CTRL_RX_IE, 0U, prv_qemu(uart)->base + UART_CTRL_OFFSET);
}

void pbl_uart_qemu_irq_handler(const struct pbl_uart_qemu *qemu) {
  if ((prv_getreg(qemu, UART_INT_OFFSET) & INT_RX_PENDING) == 0U) {
    return;
  }

  prv_putreg(qemu, UART_INT_OFFSET, INT_RX_PENDING);

  // Bytes left once reception is disabled stay in the UART for the next interrupt
  while (qemu->uart.state->rx_enabled &&
         (prv_getreg(qemu, UART_STATE_OFFSET) & STATE_RX_READY) != 0U) {
    (void)pbl_uart_receive(&qemu->uart,
                           PBL_FIELD_GET(DATA_MASK, prv_getreg(qemu, UART_DATA_OFFSET)));
  }
}

const struct pbl_uart_ops pbl_uart_qemu_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .flush = prv_flush,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};
