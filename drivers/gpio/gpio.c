/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/gpio.h>

#include <system/passert.h>

int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags) {
  PBL_ASSERTN(pbl_device_is_ready(&gpio->port->dev));

  flags |= gpio->flags;
  if ((flags & PBL_GPIO_OUTPUT_INIT_LOGICAL) && (flags & PBL_GPIO_ACTIVE_LOW) &&
      (flags & (PBL_GPIO_OUTPUT_INIT_LOW | PBL_GPIO_OUTPUT_INIT_HIGH))) {
    flags ^= PBL_GPIO_OUTPUT_INIT_LOW | PBL_GPIO_OUTPUT_INIT_HIGH;
  }
  flags &= ~PBL_GPIO_OUTPUT_INIT_LOGICAL;

  return gpio->port->ops->configure(gpio->port, gpio->pin, flags);
}

int pbl_gpio_get_raw(const struct pbl_gpio *gpio) {
  return gpio->port->ops->get(gpio->port, gpio->pin);
}

int pbl_gpio_set_raw(const struct pbl_gpio *gpio, bool level) {
  return gpio->port->ops->set(gpio->port, gpio->pin, level);
}

int pbl_gpio_get(const struct pbl_gpio *gpio) {
  int level = pbl_gpio_get_raw(gpio);

  if (level < 0) {
    return level;
  }

  return (gpio->flags & PBL_GPIO_ACTIVE_LOW) ? !level : level;
}

int pbl_gpio_set(const struct pbl_gpio *gpio, bool active) {
  return pbl_gpio_set_raw(gpio, (gpio->flags & PBL_GPIO_ACTIVE_LOW) ? !active : active);
}
