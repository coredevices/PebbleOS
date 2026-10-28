/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <board/board.h>

#define GPIO_STATE   0x00U
#define GPIO_INTCTRL 0x08U
#define GPIO_INTSTAT 0x0CU

#define GPIO_INTCTRL_EN 0x1U

#define GPIO_NUM_PINS 4U

static const struct pbl_gpio_qemu *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_qemu, port);
}

static const struct pbl_gpio_qemu *prv_intc_port(const struct pbl_intc *intc) {
  return container_of(intc, const struct pbl_gpio_qemu, intc);
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  return (flags & PBL_GPIO_OUTPUT) ? -ENOTSUP : 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return (pbl_readl(prv_port(port)->base + GPIO_STATE) & PBL_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  return -ENOTSUP;
}

static const struct pbl_gpio_port_ops s_port_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

// The peripheral flags both edges of every pin; types and masks are applied here
static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  struct pbl_gpio_qemu_state *state = prv_intc_port(intc)->state;

  if ((type & PBL_IRQ_TYPE_EDGE_BOTH) == 0U) {
    return -ENOTSUP;
  }

  PBL_WRITE_BIT(state->rising, hwirq, (type & PBL_IRQ_TYPE_EDGE_RISING) != 0U);
  PBL_WRITE_BIT(state->falling, hwirq, (type & PBL_IRQ_TYPE_EDGE_FALLING) != 0U);

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_qemu *gpio = prv_intc_port(intc);

  pbl_writel(PBL_BIT(hwirq), gpio->base + GPIO_INTSTAT);
  gpio->state->enabled |= PBL_BIT(hwirq);

  return 0;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  prv_intc_port(intc)->state->enabled &= ~PBL_BIT(hwirq);

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
};

static void prv_irq_handler(const struct pbl_gpio_qemu *gpio) {
  const struct pbl_gpio_qemu_state *state = gpio->state;
  uint32_t edges = pbl_readl(gpio->base + GPIO_INTSTAT);
  uint32_t level;
  uint32_t pending;

  pbl_writel(edges, gpio->base + GPIO_INTSTAT);
  level = pbl_readl(gpio->base + GPIO_STATE);

  pending = edges & state->enabled & ((level & state->rising) | (~level & state->falling));
  while (pending != 0U) {
    pbl_intc_dispatch(&gpio->intc, (uint32_t)__builtin_ctz(pending));
    pending &= pending - 1U;
  }
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_gpio_qemu *gpio = prv_port(container_of(dev, const struct pbl_gpio_port, dev));

  pbl_writel(PBL_BIT_MASK(GPIO_NUM_PINS), gpio->base + GPIO_INTSTAT);
  pbl_writel(GPIO_INTCTRL_EN, gpio->base + GPIO_INTCTRL);
  pbl_irq_enable(PBL_IRQN(GPIO));

  return 0;
}

static struct pbl_gpio_qemu_state s_gpio_state;

PBL_DEVICE_STATE_DEFINE(pbl_gpio_qemu_gpio);
PBL_INTC_STATE_DEFINE(pbl_gpio_qemu_gpio_intc, GPIO_NUM_PINS);
const struct pbl_gpio_qemu pbl_gpio_qemu_gpio = {
  .port = PBL_GPIO_PORT_INIT(pbl_gpio_qemu_gpio, "gpio", prv_init, nullptr, &s_port_ops,
                             GPIO_NUM_PINS, &pbl_gpio_qemu_gpio.intc),
  .intc = PBL_INTC_INIT(pbl_gpio_qemu_gpio_intc, "gpio_intc", nullptr, &pbl_gpio_qemu_gpio.port.dev,
                        &s_intc_ops),
  .base = QEMU_GPIO_BASE,
  .state = &s_gpio_state,
};
PBL_DEVICE_REGISTER(pbl_gpio_qemu_gpio, &pbl_gpio_qemu_gpio.port.dev);
PBL_DEVICE_REGISTER(pbl_gpio_qemu_gpio_intc, &pbl_gpio_qemu_gpio.intc.dev);

PBL_IRQ_CONNECT(GPIO, 6, prv_irq_handler, &pbl_gpio_qemu_gpio, 0);
