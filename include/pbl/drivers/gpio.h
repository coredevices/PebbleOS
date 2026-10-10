/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_gpio GPIO
 * @ingroup drivers
 * @brief GPIO controller class.
 *
 * A GPIO controller, an SoC port or a peripheral with GPIOs on it, is a struct pbl_gpio_port
 * device. A pin is a struct pbl_gpio: port, pin number and wiring flags (polarity, pulls, open
 * drain), so callers deal in logical levels.
 *
 * @code{.c}
 * static const struct pbl_gpio s_reset = PBL_GPIO(SF32LB_GPIO1, 28, PBL_GPIO_ACTIVE_LOW);
 *
 * pbl_gpio_configure(&s_reset, PBL_GPIO_OUTPUT_ACTIVE);
 * pbl_gpio_set(&s_reset, false);
 *
 * pbl_gpio_configure(&dev->int_gpio, PBL_GPIO_INPUT);
 * bool active = pbl_gpio_get(&dev->int_gpio) > 0;
 * @endcode
 * @{
 */

/**
 * @name Wiring flags
 * Describe the pin as routed on the board, stored in struct pbl_gpio.
 * @{
 */
/** The pin is active when low. */
#define PBL_GPIO_ACTIVE_LOW (1U << 0)
/** Enable the pull-up. */
#define PBL_GPIO_PULL_UP (1U << 1)
/** Enable the pull-down. */
#define PBL_GPIO_PULL_DOWN (1U << 2)
/** Drive the output low only. */
#define PBL_GPIO_OPEN_DRAIN (1U << 3)
/** @} */

/**
 * @name Configuration flags
 * Passed to pbl_gpio_configure(), OR'ed with the wiring flags.
 * @{
 */
/** Input. */
#define PBL_GPIO_INPUT (1U << 4)
/** Output, keeping the current level. */
#define PBL_GPIO_OUTPUT (1U << 5)
/** Physical initial level low. */
#define PBL_GPIO_OUTPUT_INIT_LOW (1U << 6)
/** Physical initial level high. */
#define PBL_GPIO_OUTPUT_INIT_HIGH (1U << 7)
/** The initial level is logical: inverted on an active-low pin. */
#define PBL_GPIO_OUTPUT_INIT_LOGICAL (1U << 8)

/** Output, physically low. */
#define PBL_GPIO_OUTPUT_LOW (PBL_GPIO_OUTPUT | PBL_GPIO_OUTPUT_INIT_LOW)
/** Output, physically high. */
#define PBL_GPIO_OUTPUT_HIGH (PBL_GPIO_OUTPUT | PBL_GPIO_OUTPUT_INIT_HIGH)
/** Output, inactive. */
#define PBL_GPIO_OUTPUT_INACTIVE (PBL_GPIO_OUTPUT_LOW | PBL_GPIO_OUTPUT_INIT_LOGICAL)
/** Output, active. */
#define PBL_GPIO_OUTPUT_ACTIVE (PBL_GPIO_OUTPUT_HIGH | PBL_GPIO_OUTPUT_INIT_LOGICAL)
/** @} */

struct pbl_gpio_port;

/** @brief Port driver operations. Levels and flags are physical. */
struct pbl_gpio_port_ops {
  /** Configure a pin. Returns 0 or a negative errno. */
  int (*configure)(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags);
  /** Read a pin. Returns 0 or 1, or a negative errno. */
  int (*get)(const struct pbl_gpio_port *port, uint8_t pin);
  /** Drive a pin. Returns 0 or a negative errno. */
  int (*set)(const struct pbl_gpio_port *port, uint8_t pin, bool level);
};

/** @brief A GPIO controller. */
struct pbl_gpio_port {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_gpio_port_ops *ops;
};

/** @brief A pin as wired on the board. */
struct pbl_gpio {
  /** Port, NULL if not connected. */
  const struct pbl_gpio_port *port;
  /** Pin number on the port. */
  uint8_t pin;
  /** Wiring flags. */
  uint8_t flags;
};

/**
 * @brief Initializer for a struct pbl_gpio.
 *
 * @param _port Port.
 * @param _pin Pin number on the port.
 * @param _flags Wiring flags.
 */
#define PBL_GPIO(_port, _pin, _flags) {.port = (_port), .pin = (_pin), .flags = (_flags)}

/**
 * @brief Check whether a pin is connected.
 *
 * @param gpio Pin.
 * @return True if the pin has a port.
 */
static inline bool pbl_gpio_is_connected(const struct pbl_gpio *gpio) {
  return gpio->port != NULL;
}

/**
 * @brief Configure a pin.
 *
 * @param gpio Pin, on a ready port.
 * @param flags Configuration flags, OR'ed with the pin's wiring flags.
 * @return 0 or a negative errno.
 */
int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags);

/**
 * @brief Read the logical level of a pin.
 *
 * @param gpio Pin.
 * @return 1 if active, 0 if not, or a negative errno.
 */
int pbl_gpio_get(const struct pbl_gpio *gpio);

/**
 * @brief Drive the logical level of a pin.
 *
 * @param gpio Pin.
 * @param active True to activate.
 * @return 0 or a negative errno.
 */
int pbl_gpio_set(const struct pbl_gpio *gpio, bool active);

/**
 * @brief Read the physical level of a pin.
 *
 * @param gpio Pin.
 * @return 1 if high, 0 if low, or a negative errno.
 */
int pbl_gpio_get_raw(const struct pbl_gpio *gpio);

/**
 * @brief Drive the physical level of a pin.
 *
 * @param gpio Pin.
 * @param level True for high.
 * @return 0 or a negative errno.
 */
int pbl_gpio_set_raw(const struct pbl_gpio *gpio, bool level);

/** @} */
