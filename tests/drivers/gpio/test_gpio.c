/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio.h>

#include <clar.h>

static uint32_t s_flags;
static bool s_level;

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  s_flags = flags;
  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return s_level;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  s_level = level;
  return 0;
}

static const struct pbl_gpio_port_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static const struct pbl_intc s_intc;

PBL_DEVICE_STATE_DEFINE(s_port);
static const struct pbl_gpio_port s_port =
    PBL_GPIO_PORT_INIT(s_port, "port", nullptr, nullptr, &s_ops, 8U, &s_intc);
PBL_DEVICE_REGISTER(s_port, &s_port.dev);

PBL_DEVICE_STATE_DEFINE(s_port_noirq);
static const struct pbl_gpio_port s_port_noirq =
    PBL_GPIO_PORT_INIT(s_port_noirq, "port_noirq", nullptr, nullptr, &s_ops, 8U, nullptr);
PBL_DEVICE_REGISTER(s_port_noirq, &s_port_noirq.dev);

void test_gpio__initialize(void) {
  s_flags = 0U;
  s_level = false;
  cl_assert_equal_i(pbl_device_init(&s_port.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_port_noirq.dev), 0);
}

void test_gpio__configure_pin_range(void) {
  const struct pbl_gpio last = PBL_GPIO(&s_port, 7, 0);
  const struct pbl_gpio beyond = PBL_GPIO(&s_port, 8, 0);

  cl_assert_equal_i(pbl_gpio_configure(&last, PBL_GPIO_INPUT), 0);
  cl_assert_equal_i(pbl_gpio_configure(&beyond, PBL_GPIO_INPUT), -EINVAL);
}

void test_gpio__configure_logical_init(void) {
  const struct pbl_gpio gpio = PBL_GPIO(&s_port, 2, PBL_GPIO_ACTIVE_LOW);

  cl_assert_equal_i(pbl_gpio_configure(&gpio, PBL_GPIO_OUTPUT_ACTIVE), 0);
  cl_assert_equal_i(s_flags, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_OUTPUT | PBL_GPIO_OUTPUT_INIT_LOW);
}

void test_gpio__logical_levels(void) {
  const struct pbl_gpio gpio = PBL_GPIO(&s_port, 2, PBL_GPIO_ACTIVE_LOW);

  cl_assert_equal_i(pbl_gpio_set(&gpio, true), 0);
  cl_assert(!s_level);
  cl_assert_equal_i(pbl_gpio_get(&gpio), 1);
  cl_assert_equal_i(pbl_gpio_get_raw(&gpio), 0);
}

void test_gpio__to_irq(void) {
  const struct pbl_gpio gpio = PBL_GPIO(&s_port, 5, PBL_GPIO_ACTIVE_LOW);
  struct pbl_irq_spec spec;

  cl_assert_equal_i(pbl_gpio_to_irq(&gpio, PBL_IRQ_TYPE_EDGE_FALLING, &spec), 0);
  cl_assert_equal_p(spec.intc, &s_intc);
  cl_assert_equal_i(spec.hwirq, 5);
  cl_assert_equal_i(spec.type, PBL_IRQ_TYPE_EDGE_FALLING);
}

void test_gpio__to_irq_unsupported(void) {
  const struct pbl_gpio gpio = PBL_GPIO(&s_port_noirq, 5, 0);
  struct pbl_irq_spec spec;

  cl_assert_equal_i(pbl_gpio_to_irq(&gpio, PBL_IRQ_TYPE_EDGE_RISING, &spec), -ENOTSUP);
}

void test_gpio__class(void) {
  cl_assert_equal_p(pbl_device_find(&pbl_gpio_port_class, "port_noirq"), &s_port_noirq.dev);
}
