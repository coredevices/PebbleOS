/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>

#include <pbl/drivers/uart/posix.h>
#include <pbl_arch_posix.h>

#include "uart_posix_bottom.h"

static UARTDevice *s_devices[UART_POSIX_NUM_CHANNELS];

struct rx {
  UARTDevice *dev;
  uint8_t c;
};

static void prv_rx_isr(void *arg) {
  const struct rx *rx = arg;
  UARTDeviceState *state = rx->dev->state;
  if (state->rx_int_enabled && state->rx_irq_handler != NULL) {
    const UARTRXErrorFlags flags = {0};
    state->rx_irq_handler(rx->dev, rx->c, &flags);
  }
}

void uart_posix_rx(enum uart_posix_channel channel, uint8_t c) {
  struct rx rx = {.dev = s_devices[channel], .c = c};
  if (rx.dev != NULL) {
    pbl_posix_irq_run(prv_rx_isr, &rx);
  }
}

void uart_init(UARTDevice *dev) {
  if (s_devices[dev->channel] == NULL) {
    s_devices[dev->channel] = dev;
    uart_posix_bottom_start((enum uart_posix_channel)dev->channel);
  }
}

void uart_init_open_drain(UARTDevice *dev) {
}

void uart_init_tx_only(UARTDevice *dev) {
}

void uart_init_rx_only(UARTDevice *dev) {
}

void uart_deinit(UARTDevice *dev) {
}

void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  dev->state->rx_irq_handler = irq_handler;
}

void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler) {
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  dev->state->rx_int_enabled = enabled;
}

void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled) {
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  uart_posix_bottom_write((enum uart_posix_channel)dev->channel, data);
}

uint8_t uart_read_byte(UARTDevice *dev) {
  return 0;
}

void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length) {
}

void uart_stop_rx_dma(UARTDevice *dev) {
}

void uart_clear_rx_dma_buffer(UARTDevice *dev) {
}

bool uart_is_rx_ready(UARTDevice *dev) {
  return false;
}

bool uart_has_rx_overrun(UARTDevice *dev) {
  return false;
}

bool uart_has_rx_framing_error(UARTDevice *dev) {
  return false;
}

bool uart_is_tx_ready(UARTDevice *dev) {
  return true;
}

bool uart_is_tx_complete(UARTDevice *dev) {
  return true;
}

void uart_wait_for_tx_complete(UARTDevice *dev) {
}

UARTRXErrorFlags uart_has_errored_out(UARTDevice *dev) {
  return (UARTRXErrorFlags){0};
}

void uart_clear_all_interrupt_flags(UARTDevice *dev) {
}
