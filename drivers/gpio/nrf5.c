/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>

static uint32_t prv_pin(const struct pbl_gpio_port *port, uint8_t pin) {
  return NRF_GPIO_PIN_MAP(container_of(port, const struct pbl_gpio_nrf5, port)->index, pin);
}

static nrf_gpio_pin_pull_t prv_pull(uint32_t flags) {
  if (flags & PBL_GPIO_PULL_UP) {
    return NRF_GPIO_PIN_PULLUP;
  }

  if (flags & PBL_GPIO_PULL_DOWN) {
    return NRF_GPIO_PIN_PULLDOWN;
  }

  return NRF_GPIO_PIN_NOPULL;
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  uint32_t abs_pin = prv_pin(port, pin);

  if (flags & PBL_GPIO_OUTPUT) {
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      nrf_gpio_pin_set(abs_pin);
    } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      nrf_gpio_pin_clear(abs_pin);
    }

    nrf_gpio_cfg(abs_pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, prv_pull(flags),
                 (flags & PBL_GPIO_OPEN_DRAIN) ? NRF_GPIO_PIN_S0D1 : NRF_GPIO_PIN_S0S1,
                 NRF_GPIO_PIN_NOSENSE);
  } else if (flags & PBL_GPIO_INPUT) {
    nrf_gpio_cfg_input(abs_pin, prv_pull(flags));
  } else {
    return -EINVAL;
  }

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return nrf_gpio_pin_read(prv_pin(port, pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  nrf_gpio_pin_write(prv_pin(port, pin), level);

  return 0;
}

static const struct pbl_gpio_port_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

#define GPIO_NRF5_DEFINE(sym, _name, _index)                                       \
  PBL_DEVICE_STATE_DEFINE(sym);                                                    \
  const struct pbl_gpio_nrf5 sym = {                                               \
    .port = {.dev = PBL_DEVICE_INIT(sym, _name, NULL, NULL, NULL), .ops = &s_ops}, \
    .index = (_index),                                                             \
  };                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.port.dev)

GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p0, "gpio_p0", 0);
#ifdef NRF_P1
GPIO_NRF5_DEFINE(pbl_gpio_nrf5_p1, "gpio_p1", 1);
#endif
