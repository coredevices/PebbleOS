/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>

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
  /** Interrupt controller of the pins, child of the port. */
  struct pbl_intc intc;
  /** Register base address. */
  uintptr_t base;
  /** Runtime state. */
  struct pbl_gpio_qemu_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_qemu pbl_gpio_qemu_gpio;
/** @endcond */

/** The QEMU GPIO port. */
#define QEMU_GPIO (&pbl_gpio_qemu_gpio.port)
/** Interrupt controller of the QEMU GPIO port. */
#define QEMU_GPIO_INTC (&pbl_gpio_qemu_gpio.intc)

/** @} */
