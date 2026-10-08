/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart UART
 * @ingroup drivers
 * @brief Serial port driver interface.
 *
 * Devices are defined by the board, with a SoC-specific @c UARTDevice. Received bytes are
 * delivered to an RX handler from the UART interrupt, also when receiving through DMA.
 *
 * @code{.c}
 * static void prv_rx(UARTDevice *dev, uint8_t data, const UARTRXErrorFlags *err_flags) {
 *   // ISR context
 * }
 *
 * uart_init(dev);
 * uart_set_baud_rate(dev, 115200);
 * uart_set_rx_interrupt_handler(dev, prv_rx);
 * uart_set_rx_interrupt_enabled(dev, true);
 *
 * uart_write_byte(dev, 'A');
 * uart_wait_for_tx_complete(dev);
 * @endcode
 * @{
 */

/** @brief UART device, defined per SoC. */
typedef const struct UARTDevice UARTDevice;

/** @brief Receive error flags. */
typedef struct UARTRXErrorFlags {
  union {
    struct {
      /** Reserved. */
      uint8_t reserved : 4;
      /** Parity error. */
      bool parity_error : 1;
      /** Receive overrun: data was lost. */
      bool overrun_error : 1;
      /** Framing error. */
      bool framing_error : 1;
      /** Noise detected. */
      bool noise_detected : 1;
    };
    /** All flags as a bit mask; non-zero if any error occurred. */
    uint8_t error_mask;
  };
} UARTRXErrorFlags;

/**
 * @brief Receive interrupt handler, called from the UART ISR for each received byte.
 *
 * @param dev Device.
 * @param data Received byte.
 * @param err_flags Errors detected with this byte.
 */
typedef void (*UARTRXInterruptHandler)(UARTDevice *dev, uint8_t data,
                                       const UARTRXErrorFlags *err_flags);
/**
 * @brief Transmit interrupt handler, called from the UART ISR when a byte can be sent.
 *
 * @param dev Device.
 */
typedef void (*UARTTXInterruptHandler)(UARTDevice *dev);

/**
 * @brief Initialize the device for receive and transmit.
 *
 * @param dev Device.
 */
void uart_init(UARTDevice *dev);

/**
 * @brief Initialize the device with open-drain instead of push-pull pins.
 *
 * Not supported on all SoCs.
 *
 * @param dev Device.
 */
void uart_init_open_drain(UARTDevice *dev);

/**
 * @brief Initialize the device for transmit only.
 *
 * @param dev Device.
 */
void uart_init_tx_only(UARTDevice *dev);

/**
 * @brief Initialize the device for receive only.
 *
 * @param dev Device.
 */
void uart_init_rx_only(UARTDevice *dev);

/**
 * @brief Deinitialize the device.
 *
 * @param dev Device.
 */
void uart_deinit(UARTDevice *dev);

/**
 * @brief Set the baud rate.
 *
 * @param dev Device.
 * @param baud_rate Baud rate in bits per second.
 */
void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate);

/**
 * @brief Set the receive interrupt handler.
 *
 * @param dev Device.
 * @param irq_handler Handler, called from the ISR for each received byte.
 */
void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler);

/**
 * @brief Set the transmit interrupt handler.
 *
 * @param dev Device.
 * @param irq_handler Handler, called from the ISR when a byte can be sent.
 */
void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler);

/**
 * @brief Enable or disable the receive interrupt.
 *
 * @param dev Device.
 * @param enabled true to enable.
 */
void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled);
/**
 * @brief Enable or disable the transmit interrupt.
 *
 * @param dev Device.
 * @param enabled true to enable.
 */
void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled);

/**
 * @brief Write a byte.
 *
 * Blocks until the transmit buffer has room.
 *
 * @param dev Device.
 * @param data Byte to write.
 */
void uart_write_byte(UARTDevice *dev, uint8_t data);

/**
 * @brief Read a byte.
 *
 * Clears the framing and overrun error flags.
 *
 * @param dev Device.
 * @return Byte read.
 */
uint8_t uart_read_byte(UARTDevice *dev);

/**
 * @brief Start receiving into a circular DMA buffer.
 *
 * The device must have DMA configured. Received bytes are still delivered to the receive
 * handler. Some SoCs require @p buffer and @p length to be aligned to the data cache line.
 *
 * @param dev Device.
 * @param buffer DMA buffer.
 * @param length Size of @p buffer in bytes.
 */
void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length);

/**
 * @brief Stop receiving through DMA.
 *
 * @param dev Device.
 */
void uart_stop_rx_dma(UARTDevice *dev);

/**
 * @brief Discard data pending in the receive DMA buffer.
 *
 * @param dev Device.
 */
void uart_clear_rx_dma_buffer(UARTDevice *dev);

/**
 * @brief Check whether a received byte is ready to be read.
 *
 * @param dev Device.
 * @return true if a byte is ready.
 */
bool uart_is_rx_ready(UARTDevice *dev);

/**
 * @brief Check for a receive overrun.
 *
 * Check before reading the received byte, as reading clears the flag.
 *
 * @param dev Device.
 * @return true on overrun.
 */
bool uart_has_rx_overrun(UARTDevice *dev);

/**
 * @brief Check for a framing error.
 *
 * Check before reading the received byte, as reading clears the flag.
 *
 * @param dev Device.
 * @return true on framing error.
 */
bool uart_has_rx_framing_error(UARTDevice *dev);

/**
 * @brief Check whether a byte can be written.
 *
 * @param dev Device.
 * @return true if the transmit buffer has room.
 */
bool uart_is_tx_ready(UARTDevice *dev);

/**
 * @brief Check whether the last byte has been fully sent.
 *
 * @param dev Device.
 * @return true if transmission is complete.
 */
bool uart_is_tx_complete(UARTDevice *dev);

/**
 * @brief Wait until the last byte has been fully sent.
 *
 * @param dev Device.
 */
void uart_wait_for_tx_complete(UARTDevice *dev);

/**
 * @brief Get the pending receive errors.
 *
 * @param dev Device.
 * @return Error flags; @c error_mask is non-zero if an error occurred.
 */
UARTRXErrorFlags uart_has_errored_out(UARTDevice *dev);

/**
 * @brief Clear all interrupt flags.
 *
 * @param dev Device.
 */
void uart_clear_all_interrupt_flags(UARTDevice *dev);

/** @} */
