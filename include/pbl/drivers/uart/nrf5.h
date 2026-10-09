/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/uart.h>

#include <board/board.h>
#include <nrfx_uarte.h>

/**
 * @defgroup drivers_uart_nrf5 nRF5
 * @ingroup drivers_uart
 * @brief nRF5 UARTE device definition.
 *
 * Receive DMA uses a timer counting received bytes to find the DMA write position.
 * @{
 */

/** @brief nRF5 UART state, owned by the driver. */
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
  /** Unused. */
  bool rx_done_pending;
  /** Receive DMA buffer, NULL when not receiving through DMA. */
  uint8_t *rx_dma_buffer;
  /** Receive DMA running; paused while the receive interrupt is disabled. */
  bool rx_dma_running;
  /** Size of the receive DMA buffer in bytes. */
  uint32_t rx_dma_length;
  /** Index of the receive sub-buffer queued next to the UARTE. */
  uint32_t rx_dma_index;
  /** Index of the receive sub-buffer being filled by DMA. */
  uint32_t rx_prod_index;
  /** Index of the receive sub-buffer being consumed. */
  uint32_t rx_cons_index;
  /** Consumed bytes in the current receive sub-buffer. */
  uint32_t rx_cons_pos;
  /** UARTE transmit cache buffer. */
  uint32_t tx_cache_buffer[8];
  /** UARTE receive cache buffer. */
  uint32_t rx_cache_buffer[8];
} UARTDeviceState;

/** @brief nRF5 UART device. */
typedef const struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** Half-duplex operation. */
  bool half_duplex;
  /** TX pin. */
  uint32_t tx_gpio;
  /** RX pin. */
  uint32_t rx_gpio;
  /** RTS pin. */
  uint32_t rts_gpio;
  /** CTS pin. */
  uint32_t cts_gpio;
  /** UARTE instance. */
  nrfx_uarte_t periph;
  /** Timer counting received bytes. */
  nrfx_timer_t counter;
} UARTDevice;

/**
 * @brief UART interrupt handler.
 *
 * Called from the IRQ handler in the board file on nRF5, and connected with PBL_IRQ_CONNECT()
 * in the board file on SF32LB and QEMU.
 *
 * @param dev Device.
 */
void uart_irq_handler(UARTDevice *dev);

/** @} */
