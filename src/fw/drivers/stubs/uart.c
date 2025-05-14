#include "drivers/uart.h"

void uart_init(UARTDevice *dev) {
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


void uart_write_byte(UARTDevice *dev, uint8_t data) {
}

uint8_t uart_read_byte(UARTDevice *dev) {
  return 0U;
}

UARTRXErrorFlags uart_has_errored_out(UARTDevice *dev) {
  UARTRXErrorFlags flags = {};
  return flags;
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
  return false;
}

bool uart_is_tx_complete(UARTDevice *dev) {
  return true;
}

void uart_wait_for_tx_complete(UARTDevice *dev) {
  while (!uart_is_tx_complete(dev)) continue;
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
}

void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler) {
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
}

void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled) {
}

void uart_clear_all_interrupt_flags(UARTDevice *dev) {
}

void uart_stop_rx_dma(UARTDevice *dev) {
}

void uart_clear_rx_dma_buffer(UARTDevice *dev) {
}
