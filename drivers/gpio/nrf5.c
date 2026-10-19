/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_gpio.h>
#include <hardware/nrf52_gpiote.h>

#define P0_NUM_PINS 32U
#define P1_NUM_PINS 16U

#define CHANNEL_FREE     0xffU
#define CHANNEL_RESERVED 0xfeU

// Absolute pin of each GPIOTE channel
static uint8_t s_channels[NRF52_GPIOTE_NUM_CHANNELS] = {
  [0 ... NRF52_GPIOTE_NUM_CHANNELS - 1] = CHANNEL_FREE,
};

static const struct pbl_gpio_nrf5 *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_nrf5, port);
}

static const struct pbl_gpio_nrf5 *prv_intc_port(const struct pbl_intc *intc) {
  return container_of(intc, const struct pbl_gpio_nrf5, intc);
}

static inline void prv_putreg(const struct pbl_gpio_nrf5 *gpio, uint32_t offset, uint32_t value) {
  pbl_writel(value, gpio->base + offset);
}

static inline uint32_t prv_getreg(const struct pbl_gpio_nrf5 *gpio, uint32_t offset) {
  return pbl_readl(gpio->base + offset);
}

static inline uint32_t prv_gpiote_getreg(uint32_t offset) {
  return pbl_readl(NRF52_GPIOTE_BASE + offset);
}

static inline void prv_gpiote_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_GPIOTE_BASE + offset);
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_nrf5 *gpio = prv_port(port);
  uint32_t cnf;

  if ((flags & (PBL_GPIO_INPUT | PBL_GPIO_OUTPUT)) == 0U) {
    return -EINVAL;
  }

  if ((flags & PBL_GPIO_PULL_UP) != 0U) {
    cnf = PBL_FIELD_PREP(GPIO_PIN_CNF_PULL_MASK, GPIO_PIN_CNF_PULL_UP);
  } else if ((flags & PBL_GPIO_PULL_DOWN) != 0U) {
    cnf = PBL_FIELD_PREP(GPIO_PIN_CNF_PULL_MASK, GPIO_PIN_CNF_PULL_DOWN);
  } else {
    cnf = PBL_FIELD_PREP(GPIO_PIN_CNF_PULL_MASK, GPIO_PIN_CNF_PULL_DISABLED);
  }

  if ((flags & PBL_GPIO_OUTPUT) != 0U) {
    if ((flags & PBL_GPIO_OUTPUT_INIT_HIGH) != 0U) {
      prv_putreg(gpio, NRF52_GPIO_OUTSET_OFFSET, PBL_BIT(pin));
    } else if ((flags & PBL_GPIO_OUTPUT_INIT_LOW) != 0U) {
      prv_putreg(gpio, NRF52_GPIO_OUTCLR_OFFSET, PBL_BIT(pin));
    }

    cnf |= GPIO_PIN_CNF_DIR_OUTPUT;
    cnf |= PBL_FIELD_PREP(GPIO_PIN_CNF_DRIVE_MASK, ((flags & PBL_GPIO_OPEN_DRAIN) != 0U)
                                                       ? GPIO_PIN_CNF_DRIVE_S0D1
                                                       : GPIO_PIN_CNF_DRIVE_S0S1);
  }

  if ((flags & PBL_GPIO_INPUT) == 0U) {
    cnf |= GPIO_PIN_CNF_INPUT_DISCONNECT;
  }

  prv_putreg(gpio, NRF52_GPIO_PIN_CNF_OFFSET(pin), cnf);

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return (prv_getreg(prv_port(port), NRF52_GPIO_IN_OFFSET) & PBL_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  prv_putreg(prv_port(port), level ? NRF52_GPIO_OUTSET_OFFSET : NRF52_GPIO_OUTCLR_OFFSET,
             PBL_BIT(pin));

  return 0;
}

static const struct pbl_gpio_port_ops s_port_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static int prv_channel_find(uint8_t abs_pin) {
  for (uint32_t ch = 0U; ch < NRF52_GPIOTE_NUM_CHANNELS; ch++) {
    if (s_channels[ch] == abs_pin) {
      return (int)ch;
    }
  }

  return -ENOENT;
}

static int prv_channel_alloc(uint8_t owner) {
  int ch = prv_channel_find(CHANNEL_FREE);

  if (ch < 0) {
    return -ENOMEM;
  }

  s_channels[ch] = owner;

  return ch;
}

int pbl_gpio_nrf5_gpiote_alloc(uint8_t *channel) {
  int ch;

  pbl_irq_lock();
  ch = prv_channel_alloc(CHANNEL_RESERVED);
  pbl_irq_unlock();

  if (ch < 0) {
    return ch;
  }

  *channel = (uint8_t)ch;

  return 0;
}

// The channel of a pin stays allocated once the line is first requested
static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_nrf5 *gpio = prv_intc_port(intc);
  uint8_t abs_pin = (uint8_t)(gpio->index * 32U + hwirq);
  uint32_t polarity;
  int ch;

  switch (type) {
    case PBL_IRQ_TYPE_EDGE_RISING:
      polarity = GPIOTE_CONFIG_POLARITY_LOTOHI;
      break;
    case PBL_IRQ_TYPE_EDGE_FALLING:
      polarity = GPIOTE_CONFIG_POLARITY_HITOLO;
      break;
    case PBL_IRQ_TYPE_EDGE_BOTH:
      polarity = GPIOTE_CONFIG_POLARITY_TOGGLE;
      break;
    default:
      return -ENOTSUP;
  }

  ch = prv_channel_find(abs_pin);
  if (ch < 0) {
    ch = prv_channel_alloc(abs_pin);
    if (ch < 0) {
      return ch;
    }
  }

  prv_gpiote_putreg(NRF52_GPIOTE_CONFIG_OFFSET(ch),
                    PBL_FIELD_PREP(GPIOTE_CONFIG_PSEL_MASK, hwirq) |
                        (gpio->index != 0U ? GPIOTE_CONFIG_PORT : 0U) |
                        PBL_FIELD_PREP(GPIOTE_CONFIG_POLARITY_MASK, polarity));

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_nrf5 *gpio = prv_intc_port(intc);
  int ch = prv_channel_find((uint8_t)(gpio->index * 32U + hwirq));

  if (ch < 0) {
    return -EINVAL;
  }

  prv_gpiote_putreg(NRF52_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);
  pbl_modifyl(GPIOTE_CONFIG_MODE_MASK,
              PBL_FIELD_PREP(GPIOTE_CONFIG_MODE_MASK, GPIOTE_CONFIG_MODE_EVENT),
              NRF52_GPIOTE_BASE + NRF52_GPIOTE_CONFIG_OFFSET(ch));
  prv_gpiote_putreg(NRF52_GPIOTE_INTENSET_OFFSET, GPIOTE_INT_IN(ch));

  return 0;
}

// Event mode keeps the input sensed, so a disabled line also leaves it
static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_nrf5 *gpio = prv_intc_port(intc);
  int ch = prv_channel_find((uint8_t)(gpio->index * 32U + hwirq));

  if (ch < 0) {
    return -EINVAL;
  }

  prv_gpiote_putreg(NRF52_GPIOTE_INTENCLR_OFFSET, GPIOTE_INT_IN(ch));
  pbl_modifyl(GPIOTE_CONFIG_MODE_MASK, 0U, NRF52_GPIOTE_BASE + NRF52_GPIOTE_CONFIG_OFFSET(ch));
  prv_gpiote_putreg(NRF52_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
};

#define GPIO_NRF5_DEFINE(sym, _name, _index, _base, _num_pins)                                    \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                   \
  const struct pbl_gpio_nrf5 sym = {                                                              \
    .port = PBL_GPIO_PORT_INIT(sym, _name, prv_init, nullptr, &s_port_ops, _num_pins, &sym.intc), \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym.port.dev, &s_intc_ops),        \
    .base = (_base),                                                                              \
    .index = (_index),                                                                            \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.port.dev);                                                        \
  PBL_DEVICE_REGISTER(sym##_intc, &sym.intc.dev)

static int prv_init(const struct pbl_device *dev);

GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p0, "gpio_p0", 0, NRF52_P0_BASE, P0_NUM_PINS);
GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p1, "gpio_p1", 1, NRF52_P1_BASE, P1_NUM_PINS);

static const struct pbl_gpio_nrf5 *const s_ports[] = {&pbl_gpio_nrf5_p0, &pbl_gpio_nrf5_p1};

static void prv_gpiote_irq_handler(void) {
  uint32_t inten = prv_gpiote_getreg(NRF52_GPIOTE_INTENSET_OFFSET);

  for (uint32_t ch = 0U; ch < NRF52_GPIOTE_NUM_CHANNELS; ch++) {
    uint8_t abs_pin = s_channels[ch];

    if ((inten & GPIOTE_INT_IN(ch)) == 0U ||
        prv_gpiote_getreg(NRF52_GPIOTE_EVENTS_IN_OFFSET(ch)) == 0U) {
      continue;
    }

    prv_gpiote_putreg(NRF52_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);
    (void)prv_gpiote_getreg(NRF52_GPIOTE_EVENTS_IN_OFFSET(ch));

    pbl_intc_dispatch(&s_ports[abs_pin / 32U]->intc, abs_pin % 32U);
  }
}

PBL_IRQ_CONNECT(GPIOTE, 7, prv_gpiote_irq_handler, , 0);

static int prv_init(const struct pbl_device *dev) {
  pbl_irq_enable(PBL_IRQN(GPIOTE));

  return 0;
}
