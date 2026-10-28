/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/uart.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart_nrf5 nRF5
 * @ingroup drivers_uart
 * @brief nRF5 UARTE device definition.
 *
 * Bytes are sent one at a time through EasyDMA. Reception runs through EasyDMA into the two halves
 * of the receive DMA buffer, which must be a power of two in size, while a TIMER in counter mode
 * counts the received bytes. The RX pin is connected when reception first starts.
 * @{
 */

/** @brief nRF5 UART state, owned by the driver. */
typedef struct UARTDeviceState {
  /** Receive handler. */
  UARTRXInterruptHandler rx_irq_handler;
  /** Receive DMA buffer, NULL when not receiving through DMA. */
  uint8_t *rx_buf;
  /** Size of the receive DMA buffer in bytes. */
  uint32_t rx_len;
  /** Bytes delivered since reception started. */
  uint32_t rx_consumed;
  /** Half of the receive DMA buffer to queue next. */
  uint8_t rx_next;
  /** Receive interrupt enabled. */
  bool rx_int_enabled;
  /** Reception running. */
  bool rx_running;
  /** PPI channel counting received bytes. */
  uint8_t ppi_ch;
} UARTDeviceState;

/** @brief nRF5 UART device. */
typedef const struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** UARTE instance index. */
  uint8_t index;
  /** Index of the TIMER instance counting received bytes. */
  uint8_t timer;
  /** TX pin. */
  struct pbl_gpio tx;
  /** RX pin. */
  struct pbl_gpio rx;
} UARTDevice;

/**
 * @brief UART interrupt handler.
 *
 * Connected with PBL_IRQ_CONNECT() in the board file.
 *
 * @param dev Device.
 */
void uart_irq_handler(UARTDevice *dev);

/** @} */
