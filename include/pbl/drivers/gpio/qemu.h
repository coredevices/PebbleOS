/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>

/**
 * @defgroup drivers_gpio_qemu QEMU GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio port of the QEMU GPIO peripheral.
 * @{
 */

/** @brief QEMU GPIO port. */
struct pbl_gpio_qemu {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Register base address. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_gpio_qemu pbl_gpio_qemu_gpio;
/** @endcond */

/** The QEMU GPIO port. */
#define QEMU_GPIO (&pbl_gpio_qemu_gpio.port)

/** @} */
