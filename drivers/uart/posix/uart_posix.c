/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "uart_posix_bottom.h"

#include <errno.h>
#include <stddef.h>

#include <pbl/drivers/uart/posix.h>
#include <pbl/util/misc.h>

#include <pbl_arch_posix.h>

static_assert(PBL_UART_POSIX_CONSOLE == UART_POSIX_CONSOLE);
static_assert(PBL_UART_POSIX_QEMU == UART_POSIX_QEMU);
static_assert(PBL_UART_POSIX_BT_HCI == UART_POSIX_BT_HCI);

static const struct pbl_uart *s_devices[UART_POSIX_NUM_CHANNELS];

struct rx {
  const struct pbl_uart *uart;
  uint8_t c;
};

static const struct pbl_uart_posix *prv_posix(const struct pbl_uart *uart) {
  return container_of(uart, const struct pbl_uart_posix, uart);
}

static void prv_rx_isr(void *arg) {
  const struct rx *rx = arg;

  (void)pbl_uart_receive(rx->uart, rx->c);
}

bool uart_posix_rx_enabled(enum uart_posix_channel channel) {
  const struct pbl_uart *uart = s_devices[channel];

  return uart != nullptr && __atomic_load_n(&uart->state->rx_enabled, __ATOMIC_RELAXED);
}

void uart_posix_rx(enum uart_posix_channel channel, uint8_t c) {
  struct rx rx = {.uart = s_devices[channel], .c = c};

  if (rx.uart != nullptr) {
    pbl_posix_irq_run(prv_rx_isr, &rx);
  }
}

static int prv_init(const struct pbl_uart *uart) {
  uint8_t channel = prv_posix(uart)->channel;

  if (channel >= UART_POSIX_NUM_CHANNELS || s_devices[channel] != nullptr) {
    return -EINVAL;
  }

  s_devices[channel] = uart;
  uart_posix_bottom_start((enum uart_posix_channel)channel);

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  if (cfg->parity != PBL_UART_PARITY_NONE || cfg->flow_ctrl != PBL_UART_FLOW_CTRL_NONE) {
    return -ENOTSUP;
  }

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  uart_posix_bottom_write((enum uart_posix_channel)prv_posix(uart)->channel, c);
}

// The bottom half polls the class state to hold bytes back
static int prv_rx_enable(const struct pbl_uart *uart) {
  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
}

const struct pbl_uart_ops pbl_uart_posix_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};
