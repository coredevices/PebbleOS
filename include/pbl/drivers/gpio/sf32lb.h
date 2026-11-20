/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_gpio_sf32lb SF32LB GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the SF32LB SoCs.
 *
 * Each port carries an @ref drivers_intc controller whose lines are the pins.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_sf32lb_pads;

struct pbl_gpio_sf32lb_state {
  uint32_t od[2];
};
/** @endcond */

/** @brief SF32LB GPIO port. */
struct pbl_gpio_sf32lb {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Pads of the series. */
  const struct pbl_gpio_sf32lb_pads *pads;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Clock. */
  struct pbl_clk clk;
  /** HPSYS always-on block base address, where wakeup pins latch. */
  uintptr_t aon;
  /** PMU base address, whose wake pins wake the SF32LB52 from hibernate. */
  uintptr_t pmuc;
  /** Runtime state. */
  struct pbl_gpio_sf32lb_state *state;
};

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_sf32lb_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_sf32lb *port;
};

extern const struct pbl_gpio_port_ops pbl_gpio_sf32lb_port_ops;
extern const struct pbl_intc_ops pbl_gpio_sf32lb_intc_ops;
int pbl_gpio_sf32lb_init(const struct pbl_device *dev);
void pbl_gpio_sf32lb_irq_handler(const struct pbl_gpio_sf32lb *gpio);
extern const struct pbl_gpio_sf32lb_pads pbl_gpio_sf32lb52_pads;
extern const struct pbl_gpio_sf32lb_pads pbl_gpio_sf32lb57_pads;
/** @endcond */

/**
 * @brief Define an SF32LB52 GPIO port and its interrupt controller, sym_intc.
 *
 * @param sym Symbol of the port, a struct pbl_gpio_port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line, a plain number.
 * @param _clk Clock, from PBL_CLK().
 * @param _num_pins Number of pins.
 * @param _aon Registers of the HPSYS always-on block, where the wake pins latch.
 * @param _pmuc Registers of the PMU, whose wake pins wake the SoC from hibernate.
 */
#define PBL_GPIO_SF32LB52_DEFINE(sym, _name, _base, _irq, _clk, _num_pins, _aon, _pmuc)            \
  extern const struct pbl_intc sym##_intc;                                                         \
  static struct pbl_gpio_sf32lb_state sym##_sf32lb_state;                                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_sf32lb, struct pbl_gpio_port, port){                    \
    .port = PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_sf32lb_init, nullptr,                          \
                               &pbl_gpio_sf32lb_port_ops, _num_pins, &sym##_intc),                 \
    .pads = &pbl_gpio_sf32lb52_pads,                                                               \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .aon = (_aon),                                                                                 \
    .pmuc = (_pmuc),                                                                               \
    .state = &sym##_sf32lb_state,                                                                  \
  };                                                                                               \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_sf32lb_intc, struct pbl_intc, intc){             \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,                \
                          &pbl_gpio_sf32lb_intc_ops),                                              \
    .port = &sym##_inst,                                                                           \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPIO_SF32LB_IRQ_PRIO, pbl_gpio_sf32lb_irq_handler, &sym##_inst, \
                      0)

/**
 * @brief Define an SF32LB57 GPIO port and its interrupt controller, sym_intc.
 *
 * @param sym Symbol of the port, a struct pbl_gpio_port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line, a plain number.
 * @param _clk Clock, from PBL_CLK().
 * @param _num_pins Number of pins.
 * @param _pmuc Registers of the PMU, whose wake pins wake the SoC from hibernate.
 */
#define PBL_GPIO_SF32LB57_DEFINE(sym, _name, _base, _irq, _clk, _num_pins, _pmuc)                  \
  extern const struct pbl_intc sym##_intc;                                                         \
  static struct pbl_gpio_sf32lb_state sym##_sf32lb_state;                                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_sf32lb, struct pbl_gpio_port, port){                    \
    .port = PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_sf32lb_init, nullptr,                          \
                               &pbl_gpio_sf32lb_port_ops, _num_pins, &sym##_intc),                 \
    .pads = &pbl_gpio_sf32lb57_pads,                                                               \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .aon = (0U),                                                                                   \
    .pmuc = (_pmuc),                                                                               \
    .state = &sym##_sf32lb_state,                                                                  \
  };                                                                                               \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_sf32lb_intc, struct pbl_intc, intc){             \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,                \
                          &pbl_gpio_sf32lb_intc_ops),                                              \
    .port = &sym##_inst,                                                                           \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPIO_SF32LB_IRQ_PRIO, pbl_gpio_sf32lb_irq_handler, &sym##_inst, \
                      0)

/** @} */
