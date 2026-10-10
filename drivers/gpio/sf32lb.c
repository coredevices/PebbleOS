/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/sf32lb.h>

static const struct pbl_gpio_sf32lb *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_sf32lb, port);
}

static int prv_init(const struct pbl_device *dev) {
  HAL_RCC_EnableModule(prv_port(container_of(dev, const struct pbl_gpio_port, dev))->module);

  return 0;
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);
  GPIO_InitTypeDef init = {
    .Pin = pin,
    .Pull = GPIO_NOPULL,
  };
  int pull;

  if (flags & PBL_GPIO_OUTPUT) {
    init.Mode = (flags & PBL_GPIO_OPEN_DRAIN) ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT;
    if (flags & (PBL_GPIO_OUTPUT_INIT_HIGH | PBL_GPIO_OUTPUT_INIT_LOW)) {
      HAL_GPIO_WritePin(gpio->regs, pin,
                        (flags & PBL_GPIO_OUTPUT_INIT_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  } else if (flags & PBL_GPIO_INPUT) {
    init.Mode = GPIO_MODE_INPUT;
  } else {
    return -EINVAL;
  }

  // Pulls are a pad property
  if (flags & PBL_GPIO_PULL_UP) {
    pull = PIN_PULLUP;
  } else if (flags & PBL_GPIO_PULL_DOWN) {
    pull = PIN_PULLDOWN;
  } else {
    pull = PIN_NOPULL;
  }

  HAL_PIN_Set(gpio->pad_base + pin, gpio->func_base + pin, pull, 1);
  HAL_GPIO_Init(gpio->regs, &init);

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return HAL_GPIO_ReadPin(prv_port(port)->regs, pin) != GPIO_PIN_RESET;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  HAL_GPIO_WritePin(prv_port(port)->regs, pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);

  return 0;
}

static const struct pbl_gpio_port_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

PBL_DEVICE_STATE_DEFINE(pbl_gpio_sf32lb_gpio1);
const struct pbl_gpio_sf32lb pbl_gpio_sf32lb_gpio1 = {
  .port =
      {
        .dev = PBL_DEVICE_INIT(pbl_gpio_sf32lb_gpio1, "gpio1", prv_init, NULL, NULL),
        .ops = &s_ops,
      },
  .regs = hwp_gpio1,
  .pad_base = PAD_PA00,
  .func_base = GPIO_A0,
  .module = RCC_MOD_GPIO1,
};
PBL_DEVICE_REGISTER(pbl_gpio_sf32lb_gpio1, &pbl_gpio_sf32lb_gpio1.port.dev);
