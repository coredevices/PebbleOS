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
/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_sf32lb_pads;
/** @endcond */

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
  /** Runtime state. */
  struct pbl_gpio_sf32lb_state *state;
};

/** @} */
