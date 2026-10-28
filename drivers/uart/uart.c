/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_uart_class, "uart");

static const struct pbl_uart *prv_uart(const struct pbl_device *dev) {
  return container_of(dev, const struct pbl_uart, dev);
}

int pbl_uart_init(const struct pbl_device *dev) {
  const struct pbl_uart *uart = prv_uart(dev);
  struct pbl_uart_state *state = uart->state;
  int ret;

  ret = uart->ops->init(uart);
  if (ret != 0) {
    return ret;
  }

  state->cfg = (struct pbl_uart_config){.baudrate = uart->baudrate};

  pbl_irq_lock();
  ret = uart->ops->configure(uart, &state->cfg);
  pbl_irq_unlock();

  return ret;
}

bool pbl_uart_receive(const struct pbl_uart *uart, uint8_t c) {
  struct pbl_uart_state *state = uart->state;

  if (!state->rx_enabled) {
    return false;
  }

  state->rx_cb(uart, c, state->rx_arg);

  return state->rx_enabled;
}

int pbl_uart_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  int ret;

  if (!pbl_device_is_ready(&uart->dev)) {
    return -ENODEV;
  }

  if (cfg->baudrate == 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();
  ret = uart->ops->configure(uart, cfg);
  if (ret == 0) {
    uart->state->cfg = *cfg;
  }
  pbl_irq_unlock();

  return ret;
}

int pbl_uart_config_get(const struct pbl_uart *uart, struct pbl_uart_config *cfg) {
  if (!pbl_device_is_ready(&uart->dev)) {
    return -ENODEV;
  }

  *cfg = uart->state->cfg;

  return 0;
}

void pbl_uart_poll_out(const struct pbl_uart *uart, uint8_t c) {
  if (!pbl_device_is_ready(&uart->dev)) {
    return;
  }

  pbl_irq_lock();
  uart->ops->poll_out(uart, c);
  pbl_irq_unlock();
}

void pbl_uart_flush(const struct pbl_uart *uart) {
  if (!pbl_device_is_ready(&uart->dev) || uart->ops->flush == nullptr) {
    return;
  }

  pbl_irq_lock();
  uart->ops->flush(uart);
  pbl_irq_unlock();
}

int pbl_uart_rx_enable(const struct pbl_uart *uart, pbl_uart_rx_callback_t cb, void *arg) {
  struct pbl_uart_state *state = uart->state;
  int ret = 0;

  if (!pbl_device_is_ready(&uart->dev)) {
    return -ENODEV;
  }

  pbl_irq_lock();
  state->rx_cb = cb;
  state->rx_arg = arg;
  if (!state->rx_enabled) {
    state->rx_enabled = true;
    ret = uart->ops->rx_enable(uart);
    if (ret != 0) {
      state->rx_enabled = false;
    }
  }
  pbl_irq_unlock();

  return ret;
}

int pbl_uart_rx_disable(const struct pbl_uart *uart) {
  struct pbl_uart_state *state = uart->state;

  if (!pbl_device_is_ready(&uart->dev)) {
    return -ENODEV;
  }

  pbl_irq_lock();
  if (state->rx_enabled) {
    uart->ops->rx_disable(uart);
    state->rx_enabled = false;
  }
  pbl_irq_unlock();

  return 0;
}
