/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_gpio.h>

#define P0_NUM_PINS 32U
#define P1_NUM_PINS 16U

static const struct pbl_gpio_nrf5 *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_nrf5, port);
}

static inline void prv_putreg(const struct pbl_gpio_nrf5 *gpio, uint32_t offset, uint32_t value) {
  pbl_writel(value, gpio->base + offset);
}

static inline uint32_t prv_getreg(const struct pbl_gpio_nrf5 *gpio, uint32_t offset) {
  return pbl_readl(gpio->base + offset);
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

#define GPIO_NRF5_DEFINE(sym, _name, _index, _base, _num_pins)                                 \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                \
  const struct pbl_gpio_nrf5 sym = {                                                           \
    .port = PBL_GPIO_PORT_INIT(sym, _name, nullptr, nullptr, &s_port_ops, _num_pins, nullptr), \
    .base = (_base),                                                                           \
    .index = (_index),                                                                         \
  };                                                                                           \
  PBL_DEVICE_REGISTER(sym, &sym.port.dev)

GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p0, "gpio_p0", 0, NRF52_P0_BASE, P0_NUM_PINS);
GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p1, "gpio_p1", 1, NRF52_P1_BASE, P1_NUM_PINS);
