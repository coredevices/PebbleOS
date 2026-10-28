/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/uart.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/nrf/ppi.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#include <hardware/nrf52_timer.h>
#include <hardware/nrf52_uarte.h>

static const struct {
  uintptr_t base;
  pbl_irq_t irqn;
} s_instances[] = {
  {NRF52_UARTE0_BASE, PBL_IRQN(UART0_UARTE0)},
  {NRF52_UARTE1_BASE, PBL_IRQN(UARTE1)},
};

static const uintptr_t s_timers[] = {
  NRF52_TIMER0_BASE, NRF52_TIMER1_BASE, NRF52_TIMER2_BASE, NRF52_TIMER3_BASE, NRF52_TIMER4_BASE,
};

static const struct {
  uint32_t baudrate;
  uint32_t reg;
} s_baudrates[] = {
  {1200U, UARTE_BAUDRATE_1200},     {2400U, UARTE_BAUDRATE_2400},
  {4800U, UARTE_BAUDRATE_4800},     {9600U, UARTE_BAUDRATE_9600},
  {14400U, UARTE_BAUDRATE_14400},   {19200U, UARTE_BAUDRATE_19200},
  {28800U, UARTE_BAUDRATE_28800},   {31250U, UARTE_BAUDRATE_31250},
  {38400U, UARTE_BAUDRATE_38400},   {56000U, UARTE_BAUDRATE_56000},
  {57600U, UARTE_BAUDRATE_57600},   {76800U, UARTE_BAUDRATE_76800},
  {115200U, UARTE_BAUDRATE_115200}, {230400U, UARTE_BAUDRATE_230400},
  {250000U, UARTE_BAUDRATE_250000}, {460800U, UARTE_BAUDRATE_460800},
  {921600U, UARTE_BAUDRATE_921600}, {1000000U, UARTE_BAUDRATE_1000000},
};

static inline uintptr_t prv_reg(UARTDevice *dev, uint32_t offset) {
  return s_instances[dev->index].base + offset;
}

static inline uint32_t prv_getreg(UARTDevice *dev, uint32_t offset) {
  return pbl_readl(prv_reg(dev, offset));
}

static inline void prv_putreg(UARTDevice *dev, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_reg(dev, offset));
}

static inline uintptr_t prv_timer_reg(UARTDevice *dev, uint32_t offset) {
  return s_timers[dev->timer] + offset;
}

static void prv_event_clear(UARTDevice *dev, uint32_t offset) {
  prv_putreg(dev, offset, 0U);
  (void)prv_getreg(dev, offset);
}

static void prv_wait_event(UARTDevice *dev, uint32_t offset) {
  while (prv_getreg(dev, offset) == 0U) {
  }
  prv_event_clear(dev, offset);
}

// PSEL registers may only be written while the UARTE is disabled
static void prv_psel_set(UARTDevice *dev, uint32_t offset, uint32_t value) {
  prv_putreg(dev, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
  prv_putreg(dev, offset, value);
  prv_putreg(dev, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);
}

void uart_init(UARTDevice *dev) {
  PBL_ASSERTN(dev->index < ARRAY_LENGTH(s_instances) && dev->timer < ARRAY_LENGTH(s_timers));

  PBL_ASSERTN(pbl_device_init(&dev->tx.port->dev) == 0);
  PBL_ASSERTN(pbl_gpio_configure(&dev->tx, PBL_GPIO_OUTPUT_HIGH) == 0);

  prv_putreg(dev, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
  prv_putreg(dev, NRF52_UARTE_PSEL_TXD_OFFSET, pbl_gpio_nrf5_pin(&dev->tx));
  prv_putreg(dev, NRF52_UARTE_PSEL_RXD_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(dev, NRF52_UARTE_PSEL_RTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(dev, NRF52_UARTE_PSEL_CTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(dev, NRF52_UARTE_CONFIG_OFFSET, 0U);
  prv_putreg(dev, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);

  pbl_writel(TIMER_MODE_LOW_POWER_COUNTER, prv_timer_reg(dev, NRF52_TIMER_MODE_OFFSET));
  pbl_writel(TIMER_BITMODE_32BIT, prv_timer_reg(dev, NRF52_TIMER_BITMODE_OFFSET));

  PBL_ASSERTN(soc_nrf_ppi_alloc(&dev->state->ppi_ch) == 0);
  soc_nrf_ppi_setup(dev->state->ppi_ch, prv_reg(dev, NRF52_UARTE_EVENTS_RXDRDY_OFFSET),
                    prv_timer_reg(dev, NRF52_TIMER_TASKS_COUNT_OFFSET), 0U);
  soc_nrf_ppi_enable(PBL_BIT(dev->state->ppi_ch));

  pbl_irq_enable(s_instances[dev->index].irqn);
}

void uart_init_tx_only(UARTDevice *dev) {
  uart_init(dev);
}

void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_baudrates); i++) {
    if (s_baudrates[i].baudrate == baud_rate) {
      prv_putreg(dev, NRF52_UARTE_BAUDRATE_OFFSET, s_baudrates[i].reg);
      return;
    }
  }

  pbl_panic();
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  pbl_irq_lock();

  prv_putreg(dev, NRF52_UARTE_TXD_PTR_OFFSET, (uintptr_t)&data);
  prv_putreg(dev, NRF52_UARTE_TXD_MAXCNT_OFFSET, 1U);
  prv_putreg(dev, NRF52_UARTE_TASKS_STARTTX_OFFSET, 1U);
  prv_wait_event(dev, NRF52_UARTE_EVENTS_ENDTX_OFFSET);

  // A started transmitter keeps the high-frequency clock running
  prv_putreg(dev, NRF52_UARTE_TASKS_STOPTX_OFFSET, 1U);
  prv_wait_event(dev, NRF52_UARTE_EVENTS_TXSTOPPED_OFFSET);

  pbl_irq_unlock();
}

void uart_wait_for_tx_complete(UARTDevice *dev) {
  // uart_write_byte() returns once the transmitter has stopped
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  dev->state->rx_irq_handler = irq_handler;
}

static void prv_rx_start(UARTDevice *dev) {
  UARTDeviceState *state = dev->state;
  uint32_t half = state->rx_len / 2U;

  if (prv_getreg(dev, NRF52_UARTE_PSEL_RXD_OFFSET) == UARTE_PSEL_DISCONNECTED) {
    PBL_ASSERTN(pbl_device_init(&dev->rx.port->dev) == 0);
    PBL_ASSERTN(pbl_gpio_configure(&dev->rx, PBL_GPIO_INPUT) == 0);
    prv_psel_set(dev, NRF52_UARTE_PSEL_RXD_OFFSET, pbl_gpio_nrf5_pin(&dev->rx));
  }

  pbl_writel(1U, prv_timer_reg(dev, NRF52_TIMER_TASKS_CLEAR_OFFSET));
  pbl_writel(1U, prv_timer_reg(dev, NRF52_TIMER_TASKS_START_OFFSET));

  state->rx_consumed = 0U;
  state->rx_next = 1U;
  state->rx_running = true;

  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);
  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXTO_OFFSET);
  prv_putreg(dev, NRF52_UARTE_RXD_PTR_OFFSET, (uintptr_t)state->rx_buf);
  prv_putreg(dev, NRF52_UARTE_RXD_MAXCNT_OFFSET, half);
  prv_putreg(dev, NRF52_UARTE_SHORTS_OFFSET, UARTE_SHORTS_ENDRX_STARTRX);
  prv_putreg(dev, NRF52_UARTE_INTENSET_OFFSET, UARTE_INT_RXSTARTED | UARTE_INT_RXDRDY);
  prv_putreg(dev, NRF52_UARTE_TASKS_STARTRX_OFFSET, 1U);
}

// A running receiver keeps the high-frequency clock on, stop it while nobody listens
static void prv_rx_stop(UARTDevice *dev) {
  dev->state->rx_running = false;

  prv_putreg(dev, NRF52_UARTE_INTENCLR_OFFSET, UARTE_INT_RXSTARTED | UARTE_INT_RXDRDY);
  prv_putreg(dev, NRF52_UARTE_SHORTS_OFFSET, 0U);
  prv_putreg(dev, NRF52_UARTE_TASKS_STOPRX_OFFSET, 1U);
  prv_wait_event(dev, NRF52_UARTE_EVENTS_RXTO_OFFSET);

  // nRF52840 anomaly 78: STOP alone leaves the TIMER drawing current
  pbl_writel(1U, prv_timer_reg(dev, NRF52_TIMER_TASKS_SHUTDOWN_OFFSET));

  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);
  pbl_irq_clear_pending(s_instances[dev->index].irqn);
}

static void prv_rx_update(UARTDevice *dev) {
  UARTDeviceState *state = dev->state;
  bool run = state->rx_int_enabled && state->rx_buf != nullptr;

  pbl_irq_lock();
  if (run && !state->rx_running) {
    prv_rx_start(dev);
  } else if (!run && state->rx_running) {
    prv_rx_stop(dev);
  }
  pbl_irq_unlock();
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  dev->state->rx_int_enabled = enabled;
  prv_rx_update(dev);
}

void uart_start_rx_dma(UARTDevice *dev, void *buffer, uint32_t length) {
  PBL_ASSERTN(length >= 2U && (length & (length - 1U)) == 0U && length / 2U <= UARTE_MAXCNT_MASK);

  dev->state->rx_buf = buffer;
  dev->state->rx_len = length;
  prv_rx_update(dev);
}

void uart_stop_rx_dma(UARTDevice *dev) {
  dev->state->rx_buf = nullptr;
  prv_rx_update(dev);
}

void uart_irq_handler(UARTDevice *dev) {
  UARTDeviceState *state = dev->state;
  const UARTRXErrorFlags err_flags = {};
  uint32_t count;

  if (prv_getreg(dev, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET) != 0U) {
    prv_event_clear(dev, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
    prv_putreg(dev, NRF52_UARTE_RXD_PTR_OFFSET,
               (uintptr_t)&state->rx_buf[state->rx_next * (state->rx_len / 2U)]);
    state->rx_next ^= 1U;
  }

  prv_event_clear(dev, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);

  pbl_writel(1U, prv_timer_reg(dev, NRF52_TIMER_TASKS_CAPTURE_OFFSET(0)));
  count = pbl_readl(prv_timer_reg(dev, NRF52_TIMER_CC_OFFSET(0)));

  // The halves are filled in turn, so byte n lands at n modulo the buffer size
  while (state->rx_running && state->rx_consumed != count) {
    uint8_t c = state->rx_buf[state->rx_consumed % state->rx_len];

    state->rx_consumed++;
    if (state->rx_irq_handler != nullptr) {
      state->rx_irq_handler(dev, c, &err_flags);
    }
  }
}
