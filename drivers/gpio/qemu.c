/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/gpio/qemu.h>
#include <pbl/util/misc.h>

#include <board/board.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// GPIO MMIO register offsets
#define GPIO_STATE  0x00 // r: bit per button
#define GPIO_OUTPUT 0x04 // w: output state bits

static uintptr_t prv_base(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_qemu, port)->base;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  if (level) {
    REG32(prv_base(port) + GPIO_OUTPUT) |= (1U << pin);
  } else {
    REG32(prv_base(port) + GPIO_OUTPUT) &= ~(1U << pin);
  }

  return 0;
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
    prv_set(port, pin, true);
  } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
    prv_set(port, pin, false);
  }

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return (REG32(prv_base(port) + GPIO_STATE) & (1U << pin)) != 0U;
}

static const struct pbl_gpio_port_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

PBL_DEVICE_STATE_DEFINE(pbl_gpio_qemu_gpio);
const struct pbl_gpio_qemu pbl_gpio_qemu_gpio = {
  .port =
      {
        .dev = PBL_DEVICE_INIT(pbl_gpio_qemu_gpio, "gpio", NULL, NULL, NULL),
        .ops = &s_ops,
      },
  .base = QEMU_GPIO_BASE,
};
PBL_DEVICE_REGISTER(pbl_gpio_qemu_gpio, &pbl_gpio_qemu_gpio.port.dev);
