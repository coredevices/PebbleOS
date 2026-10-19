/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/util/misc.h>

/**
 * @defgroup drivers_gpio_nrf5 nRF5 GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the nRF5 SoCs.
 *
 * Each port carries an @ref drivers_intc controller whose lines are the pins, backed by the GPIOTE
 * IN channels shared by all ports. Lines are edge triggered only.
 * @{
 */

/** @brief nRF5 GPIO port. */
struct pbl_gpio_nrf5 {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Interrupt controller of the pins, child of the port. */
  struct pbl_intc intc;
  /** Register base address. */
  uintptr_t base;
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
/** Interrupt controller of port P0. */
#define NRF5_GPIO_P0_INTC (&pbl_gpio_nrf5_p0.intc)
/** Interrupt controller of port P1. */
#define NRF5_GPIO_P1_INTC (&pbl_gpio_nrf5_p1.intc)

/**
 * @brief Absolute pin number of a pin, as nrfx takes it.
 *
 * @param gpio Pin on an nRF5 port.
 * @return Absolute pin number.
 */
static inline uint32_t pbl_gpio_nrf5_pin(const struct pbl_gpio *gpio) {
  const struct pbl_gpio_nrf5 *port = container_of(gpio->port, const struct pbl_gpio_nrf5, port);

  return (port->index * 32U) + gpio->pin;
}

/**
 * @brief Allocate a GPIOTE channel for use outside of the interrupt controllers.
 *
 * @param[out] channel Allocated channel.
 * @return 0 or @c -ENOMEM if all channels are in use.
 */
int pbl_gpio_nrf5_gpiote_alloc(uint8_t *channel);

/** @} */
