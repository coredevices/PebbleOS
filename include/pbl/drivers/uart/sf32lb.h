/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/uart.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart_sf32lb SF32LB
 * @ingroup drivers_uart
 * @brief SF32LB UART device definition.
 * @{
 */

/** @brief SF32LB UART state, owned by the driver. */
typedef struct UARTState {
  /** Device initialized. */
  bool initialized;
  /** Receive handler. */
  UARTRXInterruptHandler rx_irq_handler;
  /** Transmit handler. */
  UARTTXInterruptHandler tx_irq_handler;
  /** Receive interrupt enabled. */
  bool rx_int_enabled;
  /** Transmit interrupt enabled. */
  bool tx_int_enabled;
  /** Receive DMA buffer, NULL when not receiving through DMA. */
  uint8_t *rx_dma_buffer;
  /** Size of the receive DMA buffer in bytes. */
  uint32_t rx_dma_length;
  /** Next unprocessed byte in the receive DMA buffer. */
  uint32_t rx_dma_index;
  /** HAL UART handle; the board sets the instance and line settings. */
  UART_HandleTypeDef huart;
  /** HAL DMA handle for reception; the board sets the instance to enable DMA. */
  DMA_HandleTypeDef hdma;
  /** Back pointer to the device. */
  const void *dev;
} UARTDeviceState;

/** @brief SF32LB UART device. */
typedef const struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** RX pin. */
  Pinmux rx;
  /** TX pin. */
  Pinmux tx;
  /** UART interrupt. */
  IRQn_Type irqn;
  /** Receive DMA interrupt. */
  IRQn_Type dma_irqn;
} UARTDevice;

/** @cond INTERNAL_HIDDEN */
/* UART interrupt handler, documented with the nRF5 backend. */
void uart_irq_handler(UARTDevice *dev);
/** @endcond */
/**
 * @brief Receive DMA interrupt handler, connected with PBL_IRQ_CONNECT() in the board file.
 *
 * @param dev Device.
 */
void uart_dma_irq_handler(UARTDevice *dev);

/** @} */
