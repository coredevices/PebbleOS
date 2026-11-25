/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>

/**
 * @defgroup drivers_gpio_stm32 STM32
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio driver for the STM32 GPIO ports.
 *
 * A port is a gpio-controller child of the pin controller, with its own registers and clock,
 * which it keeps ungated. A pin taken as a GPIO leaves the function the pin controller gave it.
 *
 * Each port is also the interrupt controller of its pins, on edges, through EXTI: line n serves
 * pin n of one port at a time, which SYSCFG selects. A line another port holds is busy. The EXTI
 * interrupts have priority @c CONFIG_GPIO_STM32_IRQ_PRIO.
 * @{
 */

/** @brief STM32 GPIO port. */
struct pbl_gpio_stm32 {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** EXTI registers. */
  uintptr_t exti;
  /** SYSCFG registers. */
  uintptr_t syscfg;
  /** SYSCFG clock. */
  struct pbl_clk syscfg_clk;
};

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_stm32_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_stm32 *port;
};

extern const struct pbl_gpio_port_ops pbl_gpio_stm32_ops;
extern const struct pbl_intc_ops pbl_gpio_stm32_intc_ops;
int pbl_gpio_stm32_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an STM32 GPIO port.
 *
 * @param sym Symbol of the port, its interrupt controller being @c sym_intc.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _exti EXTI registers.
 * @param _syscfg SYSCFG registers.
 * @param _syscfg_clk SYSCFG clock, from PBL_CLK().
 */
#define PBL_GPIO_STM32_DEFINE(sym, _name, _base, _clk, _exti, _syscfg, _syscfg_clk)               \
  extern const struct pbl_intc sym##_intc;                                                        \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  PBL_INTC_STATE_DEFINE(sym##_intc, 16);                                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_stm32, struct pbl_gpio_port, port){                    \
    .port = PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_stm32_init, nullptr, &pbl_gpio_stm32_ops, 16, \
                               &sym##_intc),                                                      \
    .base = (_base),                                                                              \
    .clk = _clk,                                                                                  \
    .exti = (_exti),                                                                              \
    .syscfg = (_syscfg),                                                                          \
    .syscfg_clk = _syscfg_clk,                                                                    \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_stm32_intc, struct pbl_intc, intc) {            \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,               \
                          &pbl_gpio_stm32_intc_ops),                                              \
    .port = &sym##_inst,                                                                          \
  }

/** @} */
