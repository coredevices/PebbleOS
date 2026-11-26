/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/nrf54l.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/util/bits.h>

#include <hardware/nrf54l_gpio.h>
#include <hardware/nrf54l_gpiote.h>

static_assert(PBL_GPIO_NRF54L_GPIOTE_CHANNELS >= NRF54L_GPIOTE_NUM_CHANNELS);

#define CHANNEL_FREE 0xffU

#define MAX_PORTS 3U

// The ports, by index, for arming their wake pins at power-off
static const struct pbl_gpio_nrf54l *s_ports[MAX_PORTS];

static const struct pbl_gpio_nrf54l *prv_port(const struct pbl_gpio_port *port) {
  return PBL_CONTAINER_OF(port, const struct pbl_gpio_nrf54l, port);
}

static const struct pbl_gpio_nrf54l *prv_intc_port(const struct pbl_intc *intc) {
  return PBL_CONTAINER_OF(intc, const struct pbl_gpio_nrf54l_intc, intc)->port;
}

static inline void prv_putreg(const struct pbl_gpio_nrf54l *gpio, uint32_t offset, uint32_t value) {
  pbl_writel(value, gpio->base + offset);
}

static inline uint32_t prv_getreg(const struct pbl_gpio_nrf54l *gpio, uint32_t offset) {
  return pbl_readl(gpio->base + offset);
}

static inline uint32_t prv_gpiote_getreg(const struct pbl_gpio_nrf54l *gpio, uint32_t offset) {
  return pbl_readl(gpio->gpiote + offset);
}

static inline void prv_gpiote_putreg(const struct pbl_gpio_nrf54l *gpio, uint32_t offset,
                                     uint32_t value) {
  pbl_writel(value, gpio->gpiote + offset);
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_nrf54l *gpio = prv_port(port);
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
      prv_putreg(gpio, NRF54L_GPIO_OUTSET_OFFSET, PBL_BIT(pin));
    } else if ((flags & PBL_GPIO_OUTPUT_INIT_LOW) != 0U) {
      prv_putreg(gpio, NRF54L_GPIO_OUTCLR_OFFSET, PBL_BIT(pin));
    }

    cnf |= GPIO_PIN_CNF_DIR_OUTPUT;
  }

  if ((flags & PBL_GPIO_OPEN_DRAIN) != 0U) {
    cnf |= PBL_FIELD_PREP(GPIO_PIN_CNF_DRIVE1_MASK, GPIO_PIN_CNF_DRIVE1_D1);
  }

  if ((flags & PBL_GPIO_INPUT) == 0U) {
    cnf |= GPIO_PIN_CNF_INPUT_DISCONNECT;
  }

  prv_putreg(gpio, NRF54L_GPIO_PIN_CNF_OFFSET(pin), cnf);

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return (prv_getreg(prv_port(port), NRF54L_GPIO_IN_OFFSET) & PBL_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  prv_putreg(prv_port(port), level ? NRF54L_GPIO_OUTSET_OFFSET : NRF54L_GPIO_OUTCLR_OFFSET,
             PBL_BIT(pin));

  return 0;
}

const struct pbl_gpio_port_ops pbl_gpio_nrf54l_port_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static int prv_channel_find(const struct pbl_gpio_nrf54l *gpio, uint8_t pin) {
  for (uint32_t ch = 0U; ch < gpio->num_channels; ch++) {
    if (gpio->state->channels[ch] == pin) {
      return (int)ch;
    }
  }

  return -ENOENT;
}

// The channel of a pin stays allocated once the line is first requested
static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_nrf54l *gpio = prv_intc_port(intc);
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

  pbl_irq_lock();
  ch = prv_channel_find(gpio, (uint8_t)hwirq);
  if (ch < 0) {
    ch = prv_channel_find(gpio, CHANNEL_FREE);
    if (ch >= 0) {
      gpio->state->channels[ch] = (uint8_t)hwirq;
    }
  }
  pbl_irq_unlock();

  if (ch < 0) {
    return -ENOMEM;
  }

  prv_gpiote_putreg(gpio, NRF54L_GPIOTE_CONFIG_OFFSET(ch),
                    PBL_FIELD_PREP(GPIOTE_CONFIG_PSEL_MASK, hwirq) |
                        PBL_FIELD_PREP(GPIOTE_CONFIG_PORT_MASK, gpio->index) |
                        PBL_FIELD_PREP(GPIOTE_CONFIG_POLARITY_MASK, polarity));

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_nrf54l *gpio = prv_intc_port(intc);
  int ch = prv_channel_find(gpio, (uint8_t)hwirq);

  if (ch < 0) {
    return -EINVAL;
  }

  prv_gpiote_putreg(gpio, NRF54L_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);
  pbl_modifyl(GPIOTE_CONFIG_MODE_MASK,
              PBL_FIELD_PREP(GPIOTE_CONFIG_MODE_MASK, GPIOTE_CONFIG_MODE_EVENT),
              gpio->gpiote + NRF54L_GPIOTE_CONFIG_OFFSET(ch));
  prv_gpiote_putreg(gpio, NRF54L_GPIOTE_INTENSET_OFFSET, GPIOTE_INT_IN(ch));

  return 0;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_nrf54l *gpio = prv_intc_port(intc);
  int ch = prv_channel_find(gpio, (uint8_t)hwirq);

  if (ch < 0) {
    return -EINVAL;
  }

  prv_gpiote_putreg(gpio, NRF54L_GPIOTE_INTENCLR_OFFSET, GPIOTE_INT_IN(ch));
  pbl_modifyl(GPIOTE_CONFIG_MODE_MASK, 0U, gpio->gpiote + NRF54L_GPIOTE_CONFIG_OFFSET(ch));
  prv_gpiote_putreg(gpio, NRF54L_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);

  return 0;
}

static int prv_intc_set_wake(const struct pbl_intc *intc, uint32_t hwirq, bool on) {
  const struct pbl_gpio_nrf54l *gpio = prv_intc_port(intc);

  pbl_irq_lock();
  if (on) {
    gpio->state->wake |= PBL_BIT(hwirq);
  } else {
    gpio->state->wake &= ~PBL_BIT(hwirq);
  }
  pbl_irq_unlock();

  return 0;
}

const struct pbl_intc_ops pbl_gpio_nrf54l_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
  .set_wake = prv_intc_set_wake,
};

// Only SENSE wakes from System OFF; sensing the level opposite to the current one keeps a pin
// held at power-off from waking at once
static void prv_arm_wake(enum pbl_sys_off_mode mode, void *user_data) {
  for (uint32_t index = 0U; index < MAX_PORTS; index++) {
    const struct pbl_gpio_nrf54l *gpio = s_ports[index];

    if (gpio == nullptr) {
      continue;
    }

    for (uint32_t pin = 0U; pin < 32U; pin++) {
      int ch;
      bool high;

      if ((gpio->state->wake & PBL_BIT(pin)) == 0U) {
        continue;
      }

      ch = prv_channel_find(gpio, (uint8_t)pin);
      if (ch >= 0) {
        prv_gpiote_putreg(gpio, NRF54L_GPIOTE_CONFIG_OFFSET(ch), 0U);
      }

      high = (prv_getreg(gpio, NRF54L_GPIO_IN_OFFSET) & PBL_BIT(pin)) != 0U;
      pbl_modifyl(GPIO_PIN_CNF_SENSE_MASK,
                  PBL_FIELD_PREP(GPIO_PIN_CNF_SENSE_MASK,
                                 high ? GPIO_PIN_CNF_SENSE_LOW : GPIO_PIN_CNF_SENSE_HIGH),
                  gpio->base + NRF54L_GPIO_PIN_CNF_OFFSET(pin));
    }
  }
}

// Last, so that power-off handlers that end the system run first
PBL_SYS_OFF_HANDLER_DEFINE(gpio_nrf54l_wake, PBL_SYS_OFF_POWER_OFF, PBL_SYS_OFF_PRIO_LOW,
                           prv_arm_wake, nullptr);

void pbl_gpio_nrf54l_irq_handler(const struct pbl_gpio_nrf54l *gpio) {
  uint32_t inten = prv_gpiote_getreg(gpio, NRF54L_GPIOTE_INTENSET_OFFSET);

  for (uint32_t ch = 0U; ch < gpio->num_channels; ch++) {
    if ((inten & GPIOTE_INT_IN(ch)) == 0U ||
        prv_gpiote_getreg(gpio, NRF54L_GPIOTE_EVENTS_IN_OFFSET(ch)) == 0U) {
      continue;
    }

    prv_gpiote_putreg(gpio, NRF54L_GPIOTE_EVENTS_IN_OFFSET(ch), 0U);
    (void)prv_gpiote_getreg(gpio, NRF54L_GPIOTE_EVENTS_IN_OFFSET(ch));

    pbl_intc_dispatch(gpio->port.intc, gpio->state->channels[ch]);
  }
}

int pbl_gpio_nrf54l_init(const struct pbl_device *dev) {
  const struct pbl_gpio_nrf54l *gpio =
      prv_port(PBL_CONTAINER_OF(dev, const struct pbl_gpio_port, dev));

  if (gpio->num_channels > PBL_GPIO_NRF54L_GPIOTE_CHANNELS) {
    return -EINVAL;
  }

  for (uint32_t ch = 0U; ch < gpio->num_channels; ch++) {
    gpio->state->channels[ch] = CHANNEL_FREE;
  }

  if (gpio->index < MAX_PORTS) {
    s_ports[gpio->index] = gpio;
  }

  if (gpio->gpiote != 0U) {
    pbl_irq_enable(gpio->irq);
  }

  return 0;
}

#include PBL_DT_INSTANCES(nordic_nrf54l15_gpio)
