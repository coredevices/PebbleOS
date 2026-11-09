/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/irq.h>

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
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Runtime state. */
  struct pbl_gpio_qemu_state *state;
};

/** @} */
