/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_gpio_qemu QEMU GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio port of the QEMU GPIO peripheral.
 *
 * The peripheral has four input pins, the watch buttons, active high. Their interrupts are edge
 * triggered.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_qemu_state {
  uint32_t enabled;
  uint32_t rising;
  uint32_t falling;
};
/** @endcond */

/** @brief QEMU GPIO port. */
struct pbl_gpio_qemu {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Runtime state. */
  struct pbl_gpio_qemu_state *state;
};

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_qemu_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_qemu *port;
};

extern const struct pbl_gpio_port_ops pbl_gpio_qemu_port_ops;
extern const struct pbl_intc_ops pbl_gpio_qemu_intc_ops;
int pbl_gpio_qemu_init(const struct pbl_device *dev);
void pbl_gpio_qemu_irq_handler(const struct pbl_gpio_qemu *gpio);
/** @endcond */

/**
 * @brief Define a QEMU GPIO port and its interrupt controller, sym_intc.
 *
 * @param sym Symbol of the port, a struct pbl_gpio_port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line, a plain number.
 * @param _num_pins Number of pins.
 */
#define PBL_GPIO_QEMU_DEFINE(sym, _name, _base, _irq, _num_pins)                                 \
  extern const struct pbl_intc sym##_intc;                                                       \
  static struct pbl_gpio_qemu_state sym##_qemu_state;                                            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_qemu, struct pbl_gpio_port, port){                    \
    .port = PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_qemu_init, nullptr, &pbl_gpio_qemu_port_ops, \
                               _num_pins, &sym##_intc),                                          \
    .base = (_base),                                                                             \
    .irq = (_irq),                                                                               \
    .state = &sym##_qemu_state,                                                                  \
  };                                                                                             \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_qemu_intc, struct pbl_intc, intc){             \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,              \
                          &pbl_gpio_qemu_intc_ops),                                              \
    .port = &sym##_inst,                                                                         \
  };                                                                                             \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPIO_QEMU_IRQ_PRIO, pbl_gpio_qemu_irq_handler, &sym##_inst, 0)

/** @} */
