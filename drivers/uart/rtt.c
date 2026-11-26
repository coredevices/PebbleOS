/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>

#include <pbl/drivers/uart/rtt.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/timer.h>
#include <pbl/rtt/rtt.h>

static_assert(CONFIG_UART_RTT_CHANNEL < CONFIG_RTT_NUM_UP &&
                  CONFIG_UART_RTT_CHANNEL < CONFIG_RTT_NUM_DOWN,
              "the RTT port needs both an up and a down channel");

static void prv_poll(struct pbl_timer *timer);

static struct pbl_timer s_poll = PBL_TIMER_INITIALIZER(prv_poll);

static int prv_init(const struct pbl_uart *uart) {
  pbl_rtt_init();

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  (void)pbl_rtt_write(CONFIG_UART_RTT_CHANNEL, &c, 1U);
}

// Only an interrupt, as the fault handler before a reboot, waits for a probe to catch up
static void prv_flush(const struct pbl_uart *uart) {
  if (pbl_in_isr()) {
    (void)pbl_rtt_flush(CONFIG_UART_RTT_CHANNEL, CONFIG_UART_RTT_FLUSH_TIMEOUT_MS * 1000U);
  }
}

static void prv_poll(struct pbl_timer *timer) {
  const struct pbl_uart *uart = RTT_UART;
  uint8_t c;

  while (atomic_load(&uart->state->rx_enabled) &&
         pbl_rtt_read(CONFIG_UART_RTT_CHANNEL, &c, 1U) == 1U) {
    if (!pbl_uart_receive(uart, c)) {
      break;
    }
  }

  if (atomic_load(&uart->state->rx_enabled)) {
    pbl_timer_start(timer, PBL_MSEC(CONFIG_UART_RTT_POLL_MS));
  }
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  pbl_timer_start(&s_poll, PBL_MSEC(CONFIG_UART_RTT_POLL_MS));

  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
  pbl_timer_stop(&s_poll);
}

static const struct pbl_uart_ops s_ops = {
  .init = prv_init,
  .early_init = prv_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .flush = prv_flush,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

PBL_UART_STATE_DEFINE(pbl_uart_rtt);
const struct pbl_uart pbl_uart_rtt = PBL_UART_INIT(pbl_uart_rtt, "rtt", nullptr, &s_ops, 115200U);
PBL_DEVICE_REGISTER(pbl_uart_rtt, &pbl_uart_rtt.dev);
