/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/nrf54l.h>
#include <pbl/drivers/uart/nrf54l.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/nrf54l_uarte.h>

#define RX_HALF_SIZE (PBL_UART_NRF54L_RX_BUF_SIZE / 2U)

// Idle bit periods on the line that end a reception
#define RX_FRAME_TIMEOUT_BITS 20U

static_assert(RX_HALF_SIZE <= UARTE_MAXCNT_MASK);

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

static const struct pbl_uart_nrf54l *prv_uarte(const struct pbl_uart *uart) {
  return PBL_CONTAINER_OF(uart, const struct pbl_uart_nrf54l, uart);
}

static inline uint32_t prv_getreg(const struct pbl_uart_nrf54l *uarte, uint32_t offset) {
  return pbl_readl(uarte->base + offset);
}

static inline void prv_putreg(const struct pbl_uart_nrf54l *uarte, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, uarte->base + offset);
}

static void prv_event_clear(const struct pbl_uart_nrf54l *uarte, uint32_t offset) {
  prv_putreg(uarte, offset, 0U);
  (void)prv_getreg(uarte, offset);
}

static void prv_event_wait(const struct pbl_uart_nrf54l *uarte, uint32_t offset) {
  while (prv_getreg(uarte, offset) == 0U) {
  }
  prv_event_clear(uarte, offset);
}

static int prv_pin_init(const struct pbl_gpio *gpio, uint32_t flags) {
  if (pbl_device_init(&gpio->port->dev) != 0) {
    return -ENODEV;
  }

  return pbl_gpio_configure(gpio, flags);
}

static int prv_early_init(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf54l *uarte = prv_uarte(uart);
  int ret;

  ret = prv_pin_init(&uarte->tx, PBL_GPIO_OUTPUT_HIGH);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(uarte, NRF54L_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
  prv_putreg(uarte, NRF54L_UARTE_PSEL_TXD_OFFSET, pbl_gpio_nrf54l_psel(&uarte->tx));
  prv_putreg(uarte, NRF54L_UARTE_PSEL_RXD_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(uarte, NRF54L_UARTE_PSEL_RTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(uarte, NRF54L_UARTE_PSEL_CTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(uarte, NRF54L_UARTE_FRAMETIMEOUT_OFFSET, RX_FRAME_TIMEOUT_BITS);
  prv_putreg(uarte, NRF54L_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);

  // A transmitter left started, e.g. by the bootloader, keeps the high-frequency clock running
  prv_putreg(uarte, NRF54L_UARTE_TASKS_DMA_TX_STOP_OFFSET, 1U);
  prv_event_wait(uarte, NRF54L_UARTE_EVENTS_TXSTOPPED_OFFSET);

  return 0;
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf54l *uarte = prv_uarte(uart);
  int ret;

  ret = prv_early_init(uart);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(uarte->irq);

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  const struct pbl_uart_nrf54l *uarte = prv_uarte(uart);
  uint32_t config = PBL_FIELD_PREP(UARTE_CONFIG_FRAMESIZE_MASK, UARTE_CONFIG_FRAMESIZE_8BIT) |
                    UARTE_CONFIG_FRAMETIMEOUT;
  size_t i;

  if (cfg->parity == PBL_UART_PARITY_ODD || cfg->flow_ctrl != PBL_UART_FLOW_CTRL_NONE) {
    return -ENOTSUP;
  }

  for (i = 0U; i < PBL_ARRAY_SIZE(s_baudrates); i++) {
    if (s_baudrates[i].baudrate == cfg->baudrate) {
      break;
    }
  }

  if (i == PBL_ARRAY_SIZE(s_baudrates)) {
    return -ENOTSUP;
  }

  if (cfg->parity == PBL_UART_PARITY_EVEN) {
    config |= PBL_FIELD_PREP(UARTE_CONFIG_PARITY_MASK, UARTE_CONFIG_PARITY_INCL);
  }

  prv_putreg(uarte, NRF54L_UARTE_BAUDRATE_OFFSET, s_baudrates[i].reg);
  prv_putreg(uarte, NRF54L_UARTE_CONFIG_OFFSET, config);

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  const struct pbl_uart_nrf54l *uarte = prv_uarte(uart);

  prv_putreg(uarte, NRF54L_UARTE_DMA_TX_PTR_OFFSET, (uintptr_t)&c);
  prv_putreg(uarte, NRF54L_UARTE_DMA_TX_MAXCNT_OFFSET, 1U);
  prv_putreg(uarte, NRF54L_UARTE_TASKS_DMA_TX_START_OFFSET, 1U);
  prv_event_wait(uarte, NRF54L_UARTE_EVENTS_DMA_TX_END_OFFSET);

  // A started transmitter keeps the high-frequency clock running
  prv_putreg(uarte, NRF54L_UARTE_TASKS_DMA_TX_STOP_OFFSET, 1U);
  prv_event_wait(uarte, NRF54L_UARTE_EVENTS_TXSTOPPED_OFFSET);
}

// Each reception fills a half until it is full or the line idles, and the next one starts at once
// into the other half
static void prv_rx_start(const struct pbl_uart_nrf54l *uarte) {
  struct pbl_uart_nrf54l_state *state = uarte->state;

  state->rx_half = 0U;

  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_READY_OFFSET);
  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_END_OFFSET);
  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_RXTO_OFFSET);
  prv_putreg(uarte, NRF54L_UARTE_DMA_RX_PTR_OFFSET, (uintptr_t)state->rx_buf);
  prv_putreg(uarte, NRF54L_UARTE_DMA_RX_MAXCNT_OFFSET, RX_HALF_SIZE);
  prv_putreg(uarte, NRF54L_UARTE_SHORTS_OFFSET,
             UARTE_SHORTS_DMA_RX_END_DMA_RX_START | UARTE_SHORTS_FRAMETIMEOUT_DMA_RX_STOP);
  prv_putreg(uarte, NRF54L_UARTE_INTENSET_OFFSET, UARTE_INT_DMA_RX_READY | UARTE_INT_DMA_RX_END);
  prv_putreg(uarte, NRF54L_UARTE_TASKS_DMA_RX_START_OFFSET, 1U);
}

// A running receiver keeps the high-frequency clock on
static void prv_rx_stop(const struct pbl_uart_nrf54l *uarte) {
  prv_putreg(uarte, NRF54L_UARTE_INTENCLR_OFFSET, UARTE_INT_DMA_RX_READY | UARTE_INT_DMA_RX_END);
  prv_putreg(uarte, NRF54L_UARTE_SHORTS_OFFSET, 0U);
  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_RXTO_OFFSET);
  prv_putreg(uarte, NRF54L_UARTE_TASKS_DMA_RX_STOP_OFFSET, 1U);
  prv_event_wait(uarte, NRF54L_UARTE_EVENTS_RXTO_OFFSET);

  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_READY_OFFSET);
  prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_END_OFFSET);
  pbl_irq_clear_pending(uarte->irq);
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf54l *uarte = prv_uarte(uart);
  int ret;

  // PSEL may only be written while the UARTE is disabled
  if (prv_getreg(uarte, NRF54L_UARTE_PSEL_RXD_OFFSET) == UARTE_PSEL_DISCONNECTED) {
    ret = prv_pin_init(&uarte->rx, PBL_GPIO_INPUT);
    if (ret != 0) {
      return ret;
    }

    prv_putreg(uarte, NRF54L_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
    prv_putreg(uarte, NRF54L_UARTE_PSEL_RXD_OFFSET, pbl_gpio_nrf54l_psel(&uarte->rx));
    prv_putreg(uarte, NRF54L_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);
  }

  prv_rx_start(uarte);

  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
  prv_rx_stop(prv_uarte(uart));
}

void pbl_uart_nrf54l_irq_handler(const struct pbl_uart_nrf54l *uarte) {
  struct pbl_uart_nrf54l_state *state = uarte->state;

  // EasyDMA holds received bytes back until a reception ends
  if (prv_getreg(uarte, NRF54L_UARTE_EVENTS_DMA_RX_END_OFFSET) != 0U) {
    const uint8_t *half = &state->rx_buf[state->rx_half * RX_HALF_SIZE];
    uint32_t amount;

    prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_END_OFFSET);
    amount = prv_getreg(uarte, NRF54L_UARTE_DMA_RX_AMOUNT_OFFSET);
    state->rx_half ^= 1U;

    for (uint32_t i = 0U; i < amount; i++) {
      if (!pbl_uart_receive(&uarte->uart, half[i])) {
        break;
      }
    }
  }

  if (prv_getreg(uarte, NRF54L_UARTE_EVENTS_DMA_RX_READY_OFFSET) != 0U) {
    prv_event_clear(uarte, NRF54L_UARTE_EVENTS_DMA_RX_READY_OFFSET);
    prv_putreg(uarte, NRF54L_UARTE_DMA_RX_PTR_OFFSET,
               (uintptr_t)&state->rx_buf[(state->rx_half ^ 1U) * RX_HALF_SIZE]);
  }
}

const struct pbl_uart_ops pbl_uart_nrf54l_ops = {
  .init = prv_init,
  .early_init = prv_early_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_uarte)
