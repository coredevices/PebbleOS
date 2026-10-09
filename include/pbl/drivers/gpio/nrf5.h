/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/util/misc.h>

#include <hal/nrf_gpio.h>

/**
 * @defgroup drivers_gpio_nrf5 nRF5 GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the nRF5 SoCs.
 * @{
 */

/** @brief nRF5 GPIO port. */
struct pbl_gpio_nrf5 {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Port index. */
  uint8_t index;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_nrf5 pbl_gpio_nrf5_p0;
extern const struct pbl_gpio_nrf5 pbl_gpio_nrf5_p1;
/** @endcond */

/** Port P0. */
#define NRF5_GPIO_P0 (&pbl_gpio_nrf5_p0.port)
/** Port P1. */
#define NRF5_GPIO_P1 (&pbl_gpio_nrf5_p1.port)

/**
 * @brief Absolute pin number of a pin, as nrfx takes it.
 *
 * @param gpio Pin on an nRF5 port.
 * @return Absolute pin number.
 */
static inline uint32_t pbl_gpio_nrf5_pin(const struct pbl_gpio *gpio) {
  const struct pbl_gpio_nrf5 *port = container_of(gpio->port, const struct pbl_gpio_nrf5, port);

  return NRF_GPIO_PIN_MAP(port->index, gpio->pin);
}

/** @} */
