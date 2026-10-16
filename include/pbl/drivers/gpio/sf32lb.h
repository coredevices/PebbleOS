/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>

/**
 * @defgroup drivers_gpio_sf32lb SF32LB GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the SF32LB SoCs.
 *
 * Each port carries an @ref drivers_intc controller whose lines are the pins.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_sf32lb_state {
  uint32_t od[2];
};
/** @endcond */

/** @brief SF32LB GPIO port. */
struct pbl_gpio_sf32lb {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Interrupt controller of the pins, child of the port. */
  struct pbl_intc intc;
  /** Register base address. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** Runtime state. */
  struct pbl_gpio_sf32lb_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_sf32lb pbl_gpio_sf32lb_gpio1;
/** @endcond */

/** Port GPIO1 (PA). */
#define SF32LB_GPIO1 (&pbl_gpio_sf32lb_gpio1.port)
/** Interrupt controller of port GPIO1. */
#define SF32LB_GPIO1_INTC (&pbl_gpio_sf32lb_gpio1.intc)

/** @} */
