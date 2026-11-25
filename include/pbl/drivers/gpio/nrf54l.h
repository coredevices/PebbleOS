/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/misc.h>

/**
 * @defgroup drivers_gpio_nrf54l nRF54L GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the nRF54L SoCs.
 *
 * A port with a GPIOTE in its power domain carries an @ref drivers_intc controller whose lines are
 * the pins, backed by the IN channels of that GPIOTE. Lines are edge triggered only.
 * @{
 */

/** @brief Channels of a GPIOTE. */
#define PBL_GPIO_NRF54L_GPIOTE_CHANNELS 8U

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_nrf54l_state {
  // Pin of each GPIOTE channel
  uint8_t channels[PBL_GPIO_NRF54L_GPIOTE_CHANNELS];
};
/** @endcond */

/** @brief nRF54L GPIO port. */
struct pbl_gpio_nrf54l {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Driver runtime state. */
  struct pbl_gpio_nrf54l_state *state;
  /** Register base address. */
  uintptr_t base;
  /** Port number. */
  uint8_t index;
  /** Registers of the GPIOTE, or 0 without one. */
  uintptr_t gpiote;
  /** Interrupt line of the GPIOTE. */
  pbl_irq_t irq;
  /** Number of channels of the GPIOTE. */
  uint8_t num_channels;
};

/**
 * @brief Pin select value of a pin, as the peripherals take it.
 *
 * @param gpio Pin on an nRF54L port.
 * @return Pin select value.
 */
static inline uint32_t pbl_gpio_nrf54l_psel(const struct pbl_gpio *gpio) {
  const struct pbl_gpio_nrf54l *port =
      PBL_CONTAINER_OF(gpio->port, const struct pbl_gpio_nrf54l, port);

  return ((uint32_t)port->index << 5) | gpio->pin;
}

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_nrf54l_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_nrf54l *port;
};

extern const struct pbl_gpio_port_ops pbl_gpio_nrf54l_port_ops;
extern const struct pbl_intc_ops pbl_gpio_nrf54l_intc_ops;
int pbl_gpio_nrf54l_init(const struct pbl_device *dev);
void pbl_gpio_nrf54l_irq_handler(const struct pbl_gpio_nrf54l *gpio);

#define PBL_GPIO_NRF54L__FIRST(_a, ...)              _a
#define PBL_GPIO_NRF54L__GPIOTE(_gpiote, _irq, _nch) _gpiote
#define PBL_GPIO_NRF54L__IRQ(_gpiote, _irq, _nch)    _irq
#define PBL_GPIO_NRF54L__NCH(_gpiote, _irq, _nch)    _nch
/** @endcond */

/**
 * @brief Define an nRF54L GPIO port, and its interrupt controller, sym_intc, with a GPIOTE.
 *
 * @param sym Symbol of the port, a struct pbl_gpio_port.
 * @param _name Name.
 * @param _base Registers.
 * @param _index Port number.
 * @param _num_pins Number of pins.
 * @param ... Registers, interrupt line and number of channels of the GPIOTE, if the port has one.
 */
#define PBL_GPIO_NRF54L_DEFINE(sym, _name, _base, _index, _num_pins, ...)                         \
  __VA_OPT__(extern const struct pbl_intc sym##_intc;)                                            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  static struct pbl_gpio_nrf54l_state sym##_nrf54l_state;                                         \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_nrf54l, struct pbl_gpio_port, port){                   \
    .port =                                                                                       \
        PBL_GPIO_PORT_INIT(sym, _name, pbl_gpio_nrf54l_init, nullptr, &pbl_gpio_nrf54l_port_ops,  \
                           _num_pins, PBL_GPIO_NRF54L__FIRST(__VA_OPT__(&sym##_intc, ) nullptr)), \
    .state = &sym##_nrf54l_state,                                                                 \
    .base = (_base),                                                                              \
    .index = (_index),                                                                            \
    __VA_OPT__(.gpiote = PBL_GPIO_NRF54L__GPIOTE(__VA_ARGS__),                                    \
               .irq = PBL_GPIO_NRF54L__IRQ(__VA_ARGS__),                                          \
               .num_channels = PBL_GPIO_NRF54L__NCH(__VA_ARGS__), )                               \
  };                                                                                              \
  __VA_OPT__(PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                        \
             PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_nrf54l_intc, struct pbl_intc, intc){ \
               .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev,    \
                                     &pbl_gpio_nrf54l_intc_ops),                                  \
               .port = &sym##_inst,                                                               \
             };                                                                                   \
             PBL_IRQ_CONNECT_NUM(PBL_GPIO_NRF54L__IRQ(__VA_ARGS__), CONFIG_GPIO_NRF54L_IRQ_PRIO,  \
                                 pbl_gpio_nrf54l_irq_handler, &sym##_inst, 0))

/** @} */
