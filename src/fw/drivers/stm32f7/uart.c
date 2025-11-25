/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "uart_definitions.h"
#include "drivers/uart.h"

#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "system/passert.h"

#include "FreeRTOS.h"

#define STM32F7_COMPATIBLE
#include <mcu.h>
#include <mcu/interrupts.h>

// The STM32F2 standard peripheral library uses a precision of 100 which is plenty, so we'll do the
// same.
#define DIV_PRECISION (100)


// Initialization / Configuration APIs
////////////////////////////////////////////////////////////////////////////////

typedef enum UARTCR1Flags {
  UARTCR1Flags_Duplex = USART_CR1_TE | USART_CR1_RE,
  UARTCR1Flags_TE = USART_CR1_TE,
  UARTCR1Flags_RE = USART_CR1_RE,
} UARTCR1Flags;

static void prv_clear_all_errors(UARTDevice *dev) {
  dev->periph->ICR |= (USART_ICR_ORECF | USART_ICR_PECF | USART_ICR_NCF | USART_ICR_FECF);
}

static void prv_init(UARTDevice *dev, bool is_open_drain, UARTCR1Flags cr1_extra_flags) {
  // Enable peripheral clock
  periph_config_enable(dev->periph, dev->rcc_apb_periph);

  // configure GPIO
  const GPIOOType_TypeDef otype = is_open_drain ? GPIO_OType_OD : GPIO_OType_PP;
  if (dev->tx_gpio.gpio) {
    gpio_af_init(&dev->tx_gpio, otype, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);
  }
  if (dev->rx_gpio.gpio) {
    // half-duplex should only define a TX pin
    PBL_ASSERTN(!dev->half_duplex);
    gpio_af_init(&dev->rx_gpio, otype, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);
  }

  // clear any lingering errors
  prv_clear_all_errors(dev);

  // configure the UART peripheral control registers
  // - 8-bit word length
  // - no parity
  // - RX / TX enabled
  // - 1 stop bit
  // - no flow control
  dev->periph->CR1 &= ~USART_CR1_UE;
  dev->periph->CR2 = (dev->do_swap_rx_tx ? USART_CR2_SWAP : 0);
  dev->periph->CR3 = (dev->half_duplex ? USART_CR3_HDSEL : 0);

  dev->periph->CR1 = cr1_extra_flags | USART_CR1_UE;
  dev->state->initialized = true;

  // initialize the DMA request
  if (dev->rx_dma) {
    dma_request_init(dev->rx_dma);
  }
}

void uart_init(UARTDevice *dev) {
  prv_init(dev, false /* !is_open_drain */, UARTCR1Flags_Duplex);
}

void uart_init_open_drain(UARTDevice *dev) {
  prv_init(dev, true /* is_open_drain */, UARTCR1Flags_Duplex);
}

void uart_init_tx_only(UARTDevice *dev) {
  prv_init(dev, false /* !is_open_drain */, UARTCR1Flags_TE);
}

void uart_init_rx_only(UARTDevice *dev) {
  prv_init(dev, false /* !is_open_drain */, UARTCR1Flags_RE);
}

void uart_deinit(UARTDevice *dev) {
  dev->periph->CR1 &= ~USART_CR1_UE;
  periph_config_disable(dev->periph, dev->rcc_apb_periph);
  // Change the pins to be digital inputs rather than AF pins. We can't change to analog inputs
  // because those aren't 5V tolerant which these pins may need to be.
  if (dev->tx_gpio.gpio) {
    const InputConfig input_config = {
      .gpio = dev->tx_gpio.gpio,
      .gpio_pin = dev->tx_gpio.gpio_pin
    };
    gpio_input_init(&input_config);
  }
  if (dev->rx_gpio.gpio) {
    const InputConfig input_config = {
      .gpio = dev->rx_gpio.gpio,
      .gpio_pin = dev->rx_gpio.gpio_pin
    };
    gpio_input_init(&input_config);
  }
}

void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {
  PBL_ASSERTN(dev->state->initialized);

  RCC_ClocksTypeDef RCC_ClocksStatus;
  RCC_GetClocksFreq(&RCC_ClocksStatus);
  uint64_t scaled_apbclock = DIV_PRECISION;
  if ((dev->periph == USART1) || (dev->periph == USART6)) {
    scaled_apbclock *= RCC_ClocksStatus.PCLK2_Frequency;
  } else {
    scaled_apbclock *= RCC_ClocksStatus.PCLK1_Frequency;
  }

  if (dev->periph->CR1 & USART_CR1_OVER8) {
    scaled_apbclock <<= 1;
  }

  // calculate the baud rate value
  const uint32_t div = (scaled_apbclock / baud_rate);
  const uint32_t brr = (div & 0xFFF0) | ((div & 0xF) >> 1);

  // we can only change the baud rate when the UART is disabled
  dev->periph->CR1 &= ~USART_CR1_UE;
  dev->periph->BRR = brr / DIV_PRECISION;
  dev->periph->CR1 |= USART_CR1_UE;
}


// Read / Write APIs
////////////////////////////////////////////////////////////////////////////////

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  PBL_ASSERTN(dev->state->initialized);

  // wait for us to be ready to send
  while (!uart_is_tx_ready(dev)) continue;

  dev->periph->TDR = data;
}

uint8_t uart_read_byte(UARTDevice *dev) {
  // explicitly clear the error flags to match up with F4 behavior
  prv_clear_all_errors(dev);

  // read the data regardless since it will clear interrupt flags
  return dev->periph->RDR;
}

UARTRXErrorFlags uart_has_errored_out(UARTDevice *dev) {
  uint16_t errors = dev->periph->ISR;
  UARTRXErrorFlags flags = {
    .parity_error = (errors & USART_ISR_PE) != 0,
    .overrun_error = (errors & USART_ISR_ORE) != 0,
    .framing_error = (errors & USART_ISR_FE) != 0,
    .noise_detected = (errors & USART_ISR_NE) != 0,
  };

  return flags;
}

bool uart_is_rx_ready(UARTDevice *dev) {
  return dev->periph->ISR & USART_ISR_RXNE;
}

bool uart_has_rx_overrun(UARTDevice *dev) {
  return dev->periph->ISR & USART_ISR_ORE;
}

bool uart_has_rx_framing_error(UARTDevice *dev) {
  return dev->periph->ISR & USART_ISR_FE;
}

bool uart_is_tx_ready(UARTDevice *dev) {
  return dev->periph->ISR & USART_ISR_TXE;
}

bool uart_is_tx_complete(UARTDevice *dev) {
  return dev->periph->ISR & USART_ISR_TC;
}

void uart_wait_for_tx_complete(UARTDevice *dev) {
  while (!uart_is_tx_complete(dev)) continue;
}


// Interrupts
////////////////////////////////////////////////////////////////////////////////

static void prv_set_interrupt_enabled(UARTDevice *dev, bool enabled) {
  if (enabled) {
    PBL_ASSERTN(dev->state->tx_irq_handler || dev->state->rx_irq_handler);
    // enable the interrupt
    NVIC_SetPriority(dev->irq_channel, dev->irq_priority);
    NVIC_EnableIRQ(dev->irq_channel);
  } else {
    // disable the interrupt
    NVIC_DisableIRQ(dev->irq_channel);
  }
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  PBL_ASSERTN(dev->state->initialized);
  dev->state->rx_irq_handler = irq_handler;
}

void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler) {
  PBL_ASSERTN(dev->state->initialized);
  dev->state->tx_irq_handler = irq_handler;
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  PBL_ASSERTN(dev->state->initialized);
  if (enabled) {
    dev->state->rx_int_enabled = true;
    dev->periph->CR1 |= USART_CR1_RXNEIE;
    prv_set_interrupt_enabled(dev, true);
  } else {
    // disable interrupt if TX is also disabled
    prv_set_interrupt_enabled(dev, dev->state->tx_int_enabled);
    dev->periph->CR1 &= ~USART_CR1_RXNEIE;
    dev->state->rx_int_enabled = false;
  }
}

void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  PBL_ASSERTN(dev->state->initialized);
  if (enabled) {
    dev->state->tx_int_enabled = true;
    dev->periph->CR1 |= USART_CR1_TXEIE;
    prv_set_interrupt_enabled(dev, true);
  } else {
    // disable interrupt if RX is also disabled
    prv_set_interrupt_enabled(dev, dev->state->rx_int_enabled);
    dev->periph->CR1 &= ~USART_CR1_TXEIE;
    dev->state->tx_int_enabled = false;
  }
}

void uart_irq_handler(UARTDevice *dev) {
  PBL_ASSERTN(dev->state->initialized);
  bool should_context_switch = false;
  if (dev->state->rx_irq_handler && dev->state->rx_int_enabled) {
    const UARTRXErrorFlags err_flags = {
      .overrun_error = uart_has_rx_overrun(dev),
      .framing_error = uart_has_rx_framing_error(dev),
    };
    if (dev->state->rx_dma_buffer) {
      // process bytes from the DMA buffer
      const uint32_t dma_length = dev->state->rx_dma_length;
      const uint32_t next_idx = dma_length - dma_request_get_current_data_counter(dev->rx_dma);
      // make sure we didn't underflow the index
      PBL_ASSERTN(next_idx < dma_length);
      while (dev->state->rx_dma_index != next_idx) {
        const uint8_t data = dev->state->rx_dma_buffer[dev->state->rx_dma_index];
        if (dev->state->rx_irq_handler(dev, data, &err_flags)) {
          should_context_switch = true;
        }
        if (++dev->state->rx_dma_index == dma_length) {
          dev->state->rx_dma_index = 0;
        }
      }
      // explicitly clear error flags since we're not reading from the data register
      uart_clear_all_interrupt_flags(dev);
    } else {
      const bool has_byte = uart_is_rx_ready(dev);
      // read the data register regardless to clear the error flags
      const uint8_t data = uart_read_byte(dev);
      if (has_byte) {
        if (dev->state->rx_irq_handler(dev, data, &err_flags)) {
          should_context_switch = true;
        }
      }
    }
  }
  if (dev->state->tx_irq_handler && dev->state->tx_int_enabled && uart_is_tx_ready(dev)) {
    if (dev->state->tx_irq_handler(dev)) {
      should_context_switch = true;
    }
  }
  portEND_SWITCHING_ISR(should_context_switch);
}

void uart_clear_all_interrupt_flags(UARTDevice *dev) {
  dev->periph->RQR |= USART_RQR_RXFRQ;
  dev->periph->ICR |= USART_ICR_ORECF;
}


// DMA
////////////////////////////////////////////////////////////////////////////////

void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length) {
  dev->periph->CR3 |= USART_CR3_DMAR;
  dma_request_start_circular(dev->rx_dma, buffer, (void *)&dev->periph->RDR, length, NULL, NULL);
  dev->state->rx_dma_index = 0;
  dev->state->rx_dma_length = length;
  dev->state->rx_dma_buffer = buffer;
}

void uart_stop_rx_dma(UARTDevice *dev) {
  dev->state->rx_dma_buffer = NULL;
  dev->state->rx_dma_length = 0;
  dma_request_stop(dev->rx_dma);
  dev->periph->CR3 &= ~USART_CR3_DMAR;
}

void uart_clear_rx_dma_buffer(UARTDevice *dev) {
  dev->state->rx_dma_index = dev->state->rx_dma_length -
                             dma_request_get_current_data_counter(dev->rx_dma);
}
