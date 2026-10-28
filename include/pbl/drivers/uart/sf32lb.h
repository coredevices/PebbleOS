/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart_sf32lb SF32LB
 * @ingroup drivers_uart
 * @brief SF32LB USART device definition.
 *
 * Bytes are sent polled. Received bytes come from the receive interrupt, or with a receive DMA
 * channel, from a cyclic transfer into the receive DMA buffer drained on line idle and on each
 * half of the buffer. The receive DMA buffer must be aligned to, and a multiple of, the data cache
 * line. Deep sleep is blocked while reception is enabled.
 * @{
 */

/** @brief SF32LB UART state, owned by the driver. */
typedef struct UARTDeviceState {
  /** Receive handler. */
  UARTRXInterruptHandler rx_irq_handler;
  /** Receive DMA buffer, NULL when not receiving through DMA. */
  uint8_t *rx_buf;
  /** Size of the receive DMA buffer in bytes. */
  uint32_t rx_len;
  /** Next unprocessed byte in the receive DMA buffer. */
  uint32_t rx_pos;
  /** Receive interrupt enabled. */
  bool rx_int_enabled;
  /** A byte was written since the last wait for completion. */
  bool tx_pending;
} UARTDeviceState;

/** @brief SF32LB UART device. */
typedef const struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** Receive DMA channel, or NULL. */
  const struct pbl_dma_chan *rx_dma;
  /** USART instance, from 1. */
  uint8_t index;
} UARTDevice;

/**
 * @brief UART interrupt handler, connected with PBL_IRQ_CONNECT() in the board file.
 *
 * @param dev Device.
 */
void uart_irq_handler(UARTDevice *dev);

/** @} */
