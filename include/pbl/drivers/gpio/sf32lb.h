/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/gpio.h>
#include <pbl/util/misc.h>

#include <bf0_hal.h>

/**
 * @defgroup drivers_gpio_sf32lb SF32LB GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the SF32LB SoCs.
 * @{
 */

/** @brief SF32LB GPIO port. */
struct pbl_gpio_sf32lb {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Registers. */
  GPIO_TypeDef *regs;
  /** Pad of pin 0. */
  int pad_base;
  /** GPIO pin function of pin 0. */
  pin_function func_base;
  /** Clock gate. */
  RCC_MODULE_TYPE module;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_sf32lb pbl_gpio_sf32lb_gpio1;
/** @endcond */

/** Port GPIO1 (PA). */
#define SF32LB_GPIO1 (&pbl_gpio_sf32lb_gpio1.port)

/**
 * @brief Registers of the port a pin is on, for HAL calls.
 *
 * @param gpio Pin on an SF32LB port.
 * @return Port registers.
 */
static inline GPIO_TypeDef *pbl_gpio_sf32lb_regs(const struct pbl_gpio *gpio) {
  return container_of(gpio->port, const struct pbl_gpio_sf32lb, port)->regs;
}

/** @} */
