/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/nrf/ppi.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/nrf52_timer.h>
#include <hardware/nrf52_uarte.h>

#define RX_HALF_SIZE (PBL_UART_NRF5_RX_BUF_SIZE / 2U)

// The received byte count runs free, so it must wrap at a multiple of the buffer size
static_assert((PBL_UART_NRF5_RX_BUF_SIZE & (PBL_UART_NRF5_RX_BUF_SIZE - 1U)) == 0U);
static_assert(RX_HALF_SIZE <= UARTE_MAXCNT_MASK);

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

static const struct pbl_uart_nrf5 *prv_nrf5(const struct pbl_uart *uart) {
  return container_of(uart, const struct pbl_uart_nrf5, uart);
}

static inline uintptr_t prv_reg(const struct pbl_uart_nrf5 *nrf5, uint32_t offset) {
  return s_instances[nrf5->index].base + offset;
}

static inline uint32_t prv_getreg(const struct pbl_uart_nrf5 *nrf5, uint32_t offset) {
  return pbl_readl(prv_reg(nrf5, offset));
}

static inline void prv_putreg(const struct pbl_uart_nrf5 *nrf5, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_reg(nrf5, offset));
}

static inline uintptr_t prv_timer_reg(const struct pbl_uart_nrf5 *nrf5, uint32_t offset) {
  return s_timers[nrf5->timer] + offset;
}

static void prv_event_clear(const struct pbl_uart_nrf5 *nrf5, uint32_t offset) {
  prv_putreg(nrf5, offset, 0U);
  (void)prv_getreg(nrf5, offset);
}

static void prv_event_wait(const struct pbl_uart_nrf5 *nrf5, uint32_t offset) {
  while (prv_getreg(nrf5, offset) == 0U) {
  }
  prv_event_clear(nrf5, offset);
}

static int prv_pin_init(const struct pbl_gpio *gpio, uint32_t flags) {
  if (pbl_device_init(&gpio->port->dev) != 0) {
    return -ENODEV;
  }

  return pbl_gpio_configure(gpio, flags);
}

static int prv_early_init(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);
  int ret;

  if (nrf5->index >= ARRAY_LENGTH(s_instances) || nrf5->timer >= ARRAY_LENGTH(s_timers)) {
    return -EINVAL;
  }

  ret = prv_pin_init(&nrf5->tx, PBL_GPIO_OUTPUT_HIGH);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(nrf5, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
  prv_putreg(nrf5, NRF52_UARTE_PSEL_TXD_OFFSET, pbl_gpio_nrf5_pin(&nrf5->tx));
  prv_putreg(nrf5, NRF52_UARTE_PSEL_RXD_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(nrf5, NRF52_UARTE_PSEL_RTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(nrf5, NRF52_UARTE_PSEL_CTS_OFFSET, UARTE_PSEL_DISCONNECTED);
  prv_putreg(nrf5, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);

  return 0;
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);
  int ret;

  ret = prv_early_init(uart);
  if (ret != 0) {
    return ret;
  }

  ret = soc_nrf_ppi_alloc(&nrf5->state->ppi_ch);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(TIMER_MODE_LOW_POWER_COUNTER, prv_timer_reg(nrf5, NRF52_TIMER_MODE_OFFSET));
  pbl_writel(TIMER_BITMODE_32BIT, prv_timer_reg(nrf5, NRF52_TIMER_BITMODE_OFFSET));

  soc_nrf_ppi_setup(nrf5->state->ppi_ch, prv_reg(nrf5, NRF52_UARTE_EVENTS_RXDRDY_OFFSET),
                    prv_timer_reg(nrf5, NRF52_TIMER_TASKS_COUNT_OFFSET), 0U);
  soc_nrf_ppi_enable(PBL_BIT(nrf5->state->ppi_ch));

  pbl_irq_enable(s_instances[nrf5->index].irqn);

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);
  uint32_t config = 0U;
  size_t i;

  if (cfg->parity == PBL_UART_PARITY_ODD || cfg->flow_ctrl != PBL_UART_FLOW_CTRL_NONE) {
    return -ENOTSUP;
  }

  for (i = 0U; i < ARRAY_LENGTH(s_baudrates); i++) {
    if (s_baudrates[i].baudrate == cfg->baudrate) {
      break;
    }
  }

  if (i == ARRAY_LENGTH(s_baudrates)) {
    return -ENOTSUP;
  }

  if (cfg->parity == PBL_UART_PARITY_EVEN) {
    config = PBL_FIELD_PREP(UARTE_CONFIG_PARITY_MASK, UARTE_CONFIG_PARITY_INCL);
  }

  prv_putreg(nrf5, NRF52_UARTE_BAUDRATE_OFFSET, s_baudrates[i].reg);
  prv_putreg(nrf5, NRF52_UARTE_CONFIG_OFFSET, config);

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);

  prv_putreg(nrf5, NRF52_UARTE_TXD_PTR_OFFSET, (uintptr_t)&c);
  prv_putreg(nrf5, NRF52_UARTE_TXD_MAXCNT_OFFSET, 1U);
  prv_putreg(nrf5, NRF52_UARTE_TASKS_STARTTX_OFFSET, 1U);
  prv_event_wait(nrf5, NRF52_UARTE_EVENTS_ENDTX_OFFSET);

  // A started transmitter keeps the high-frequency clock running
  prv_putreg(nrf5, NRF52_UARTE_TASKS_STOPTX_OFFSET, 1U);
  prv_event_wait(nrf5, NRF52_UARTE_EVENTS_TXSTOPPED_OFFSET);
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);
  struct pbl_uart_nrf5_state *state = nrf5->state;
  int ret;

  // PSEL may only be written while the UARTE is disabled
  if (prv_getreg(nrf5, NRF52_UARTE_PSEL_RXD_OFFSET) == UARTE_PSEL_DISCONNECTED) {
    ret = prv_pin_init(&nrf5->rx, PBL_GPIO_INPUT);
    if (ret != 0) {
      return ret;
    }

    prv_putreg(nrf5, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_DISABLED);
    prv_putreg(nrf5, NRF52_UARTE_PSEL_RXD_OFFSET, pbl_gpio_nrf5_pin(&nrf5->rx));
    prv_putreg(nrf5, NRF52_UARTE_ENABLE_OFFSET, UARTE_ENABLE_ENABLED);
  }

  pbl_writel(1U, prv_timer_reg(nrf5, NRF52_TIMER_TASKS_CLEAR_OFFSET));
  pbl_writel(1U, prv_timer_reg(nrf5, NRF52_TIMER_TASKS_START_OFFSET));

  state->rx_consumed = 0U;
  state->rx_next = 1U;

  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);
  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXTO_OFFSET);
  prv_putreg(nrf5, NRF52_UARTE_RXD_PTR_OFFSET, (uintptr_t)state->rx_buf);
  prv_putreg(nrf5, NRF52_UARTE_RXD_MAXCNT_OFFSET, RX_HALF_SIZE);
  prv_putreg(nrf5, NRF52_UARTE_SHORTS_OFFSET, UARTE_SHORTS_ENDRX_STARTRX);
  prv_putreg(nrf5, NRF52_UARTE_INTENSET_OFFSET, UARTE_INT_RXSTARTED | UARTE_INT_RXDRDY);
  prv_putreg(nrf5, NRF52_UARTE_TASKS_STARTRX_OFFSET, 1U);

  return 0;
}

// A running receiver keeps the high-frequency clock on
static void prv_rx_disable(const struct pbl_uart *uart) {
  const struct pbl_uart_nrf5 *nrf5 = prv_nrf5(uart);

  prv_putreg(nrf5, NRF52_UARTE_INTENCLR_OFFSET, UARTE_INT_RXSTARTED | UARTE_INT_RXDRDY);
  prv_putreg(nrf5, NRF52_UARTE_SHORTS_OFFSET, 0U);
  prv_putreg(nrf5, NRF52_UARTE_TASKS_STOPRX_OFFSET, 1U);
  prv_event_wait(nrf5, NRF52_UARTE_EVENTS_RXTO_OFFSET);

  // nRF52840 anomaly 78: STOP alone leaves the TIMER drawing current
  pbl_writel(1U, prv_timer_reg(nrf5, NRF52_TIMER_TASKS_SHUTDOWN_OFFSET));

  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);
  pbl_irq_clear_pending(s_instances[nrf5->index].irqn);
}

void pbl_uart_nrf5_irq_handler(const struct pbl_uart_nrf5 *nrf5) {
  struct pbl_uart_nrf5_state *state = nrf5->state;
  uint32_t count;

  if (prv_getreg(nrf5, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET) != 0U) {
    prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXSTARTED_OFFSET);
    prv_putreg(nrf5, NRF52_UARTE_RXD_PTR_OFFSET,
               (uintptr_t)&state->rx_buf[state->rx_next * RX_HALF_SIZE]);
    state->rx_next ^= 1U;
  }

  prv_event_clear(nrf5, NRF52_UARTE_EVENTS_RXDRDY_OFFSET);

  pbl_writel(1U, prv_timer_reg(nrf5, NRF52_TIMER_TASKS_CAPTURE_OFFSET(0)));
  count = pbl_readl(prv_timer_reg(nrf5, NRF52_TIMER_CC_OFFSET(0)));

  // The halves fill in turn, so byte n lands at n modulo the buffer size
  while (state->rx_consumed != count) {
    uint8_t c = state->rx_buf[state->rx_consumed % PBL_UART_NRF5_RX_BUF_SIZE];

    state->rx_consumed++;
    if (!pbl_uart_receive(&nrf5->uart, c)) {
      break;
    }
  }
}

const struct pbl_uart_ops pbl_uart_nrf5_ops = {
  .init = prv_init,
  .early_init = prv_early_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};
