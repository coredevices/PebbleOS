/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <unistd.h>

#include <pbl/drivers/uart/posix.h>
#include <pbl_arch_posix.h>

#include "uart_posix_bottom.h"

static UARTDevice *s_console;

static void prv_rx_isr(void *arg) {
  const uint8_t *c = arg;
  UARTDeviceState *state = s_console->state;
  if (state->rx_int_enabled && state->rx_irq_handler != NULL) {
    const UARTRXErrorFlags flags = {0};
    state->rx_irq_handler(s_console, *c, &flags);
  }
}

void uart_posix_console_rx(uint8_t c) {
  if (s_console != NULL) {
    pbl_posix_irq_run(prv_rx_isr, &c);
  }
}

void uart_init(UARTDevice *dev) {
  if (dev->console && s_console == NULL) {
    s_console = dev;
    uart_posix_bottom_console_start();
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
  (void)write(dev->fd, &data, 1);
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
