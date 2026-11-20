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
  /** Register base address. */
  uintptr_t base;
  /** Port index. */
  uint8_t index;
  /** Registers of the GPIOTE. */
  uintptr_t gpiote;
};

/**
 * @brief Absolute pin number of a pin, as nrfx takes it.
 *
 * @param gpio Pin on an nRF5 port.
 * @return Absolute pin number.
 */
static inline uint32_t pbl_gpio_nrf5_pin(const struct pbl_gpio *gpio) {
  const struct pbl_gpio_nrf5 *port = PBL_CONTAINER_OF(gpio->port, const struct pbl_gpio_nrf5, port);

  return (port->index * 32U) + gpio->pin;
}

/**
 * @brief Allocate a GPIOTE channel for use outside of the interrupt controllers.
 *
 * @param[out] channel Allocated channel.
 * @return 0 or @c -ENOMEM if all channels are in use.
 */
int pbl_gpio_nrf5_gpiote_alloc(uint8_t *channel);

/** @cond INTERNAL_HIDDEN */
// The ports share a 4 KiB peripheral, each at its number times 0x300
#define PBL_GPIO_NRF5__INDEX(_base) (((_base) & 0xFFFU) / 0x300U)

struct pbl_gpio_nrf5_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_nrf5 *port;
};

extern const struct pbl_gpio_port_ops pbl_gpio_nrf5_port_ops;
extern const struct pbl_intc_ops pbl_gpio_nrf5_intc_ops;
int pbl_gpio_nrf5_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an nRF5 GPIO port and its interrupt controller, sym_intc.
 *
 * @param sym Symbol of the port, a struct pbl_gpio_port.
 * @param _name Name.
 * @param _base Registers.
 * @param _gpiote Registers of the GPIOTE.
 * @param _num_pins Number of pins.
 */
#define PBL_GPIO_NRF5_DEFINE(sym, _name, _base, _gpiote, _num_pins)                              \
  extern const struct pbl_intc sym##_intc;                                                       \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_nrf5, struct pbl_gpio_port, port){                    \
    .port = PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_nrf5_init, nullptr, &pbl_gpio_nrf5_port_ops, \
                               _num_pins, &sym##_intc),                                          \
    .base = (_base),                                                                             \
    .index = PBL_GPIO_NRF5__INDEX(_base),                                                        \
    .gpiote = (_gpiote),                                                                         \
  };                                                                                             \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_nrf5_intc, struct pbl_intc, intc) {            \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,              \
                          &pbl_gpio_nrf5_intc_ops),                                              \
    .port = &sym##_inst,                                                                         \
  }

/** @} */
