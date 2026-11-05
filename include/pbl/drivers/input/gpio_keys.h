/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/gpio.h>
#include <pbl/kernel/timer.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_input_gpio_keys GPIO keys
 * @ingroup drivers
 * @brief Keys on GPIOs, reported as @ref input key events.
 *
 * Each edge of a key restarts its debounce timer; when the timer expires the key is read and, if
 * it changed, reported. A key must therefore be stable for its debounce interval to be reported.
 * Keys held at initialization are reported once their debounce interval has elapsed.
 *
 * The configuration follows the devicetree gpio-keys binding, one key per child node:
 *
 * @code{.c}
 * PBL_GPIO_KEYS_DEFINE(BUTTONS, "buttons",
 *   PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(NRF5_GPIO_P0, 28, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP),
 *                     PBL_INPUT_KEY_BACK, 40),
 *   ...);
 * @endcode
 * @{
 */

/** @brief A key. */
struct pbl_gpio_keys_key {
  /** Name, @c label. */
  const char *label;
  /** GPIO, @c gpios; reads active while pressed. */
  struct pbl_gpio gpio;
  /** @ref PBL_INPUT_EV_KEY code, @c linux,code. */
  uint16_t code;
  /** Time the key must be stable to be reported, in ms, @c debounce-interval. */
  uint16_t debounce_interval_ms;
};

/** @cond INTERNAL_HIDDEN */
struct pbl_gpio_keys_key_state {
  const struct pbl_gpio_keys_key *key;
  struct pbl_timer timer;
  bool pressed;
};
/** @endcond */

/** @brief Keys on GPIOs. */
struct pbl_gpio_keys {
  /** Device. */
  struct pbl_device dev;
  /** Keys. */
  const struct pbl_gpio_keys_key *keys;
  /** Runtime state of each key. */
  struct pbl_gpio_keys_key_state *state;
  /** Number of keys. */
  uint8_t num_keys;
};

/** @cond INTERNAL_HIDDEN */
int pbl_gpio_keys_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Initializer for a key.
 *
 * @param _label Name.
 * @param _gpio GPIO, a @ref PBL_GPIO.
 * @param _code @ref PBL_INPUT_EV_KEY code.
 * @param _debounce_interval_ms Debounce interval in ms.
 */
#define PBL_GPIO_KEYS_KEY(_label, _gpio, _code, _debounce_interval_ms) \
  {                                                                    \
    .label = (_label),                                                 \
    .gpio = _gpio,                                                     \
    .code = (_code),                                                   \
    .debounce_interval_ms = (_debounce_interval_ms),                   \
  }

/**
 * @brief Define and register a set of keys.
 *
 * @param sym Symbol of the struct pbl_gpio_keys.
 * @param _name Device name.
 * @param ... Keys, each a @ref PBL_GPIO_KEYS_KEY.
 */
#define PBL_GPIO_KEYS_DEFINE(sym, _name, ...)                                      \
  static const struct pbl_gpio_keys_key sym##_keys[] = {__VA_ARGS__};              \
  static struct pbl_gpio_keys_key_state sym##_key_state[ARRAY_LENGTH(sym##_keys)]; \
  PBL_DEVICE_STATE_DEFINE(sym);                                                    \
  const struct pbl_gpio_keys sym = {                                               \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_gpio_keys_init, nullptr, nullptr),      \
    .keys = sym##_keys,                                                            \
    .state = sym##_key_state,                                                      \
    .num_keys = ARRAY_LENGTH(sym##_keys),                                          \
  };                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/** @} */
