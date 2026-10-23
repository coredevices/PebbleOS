/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_pinctrl Pin control
 * @ingroup drivers
 * @brief Pin controller class.
 *
 * A pin controller is a struct pbl_pinctrl_controller device that routes peripheral signals to
 * pins and sets their pad options. A pin is one 32-bit word in an encoding defined by the
 * controller header, and a state is a list of pins defined by the board. A consumer holds a
 * struct pbl_pinctrl with its default state, an optional sleep state, and its register base,
 * which controllers without a pin multiplexer use to fill the peripheral's own pin select
 * registers. It selects the default state before using the peripheral, and the sleep state, if
 * any, after powering it down:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_uart1_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA19_USART1_TXD | ...,
 *                          SF32LB52_PA18_USART1_RXD | ...);
 *
 * .pinctrl = PBL_PINCTRL(&s_uart1_default, nullptr, 0),
 *
 * pbl_pinctrl_select_state(&dev->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
 * @endcode
 *
 * Controllers need no initialization, so states can be selected before the devices are
 * initialized, e.g. for the debug console.
 * @{
 */

struct pbl_pinctrl_controller;

/** @brief Class of the pin controllers. */
extern const struct pbl_device_class pbl_pinctrl_class;

/** @brief Pin controller driver operations. */
struct pbl_pinctrl_controller_ops {
  /** Configure pins. @p reg is the consumer's register base. Returns 0 or a negative errno. */
  int (*configure)(const struct pbl_pinctrl_controller *ctrl, const uint32_t *pins,
                   uint8_t num_pins, uintptr_t reg);
  /** Optional. Read the raw configuration register of a pad, for diagnostics. Returns 0 or a
   * negative errno. */
  int (*get)(const struct pbl_pinctrl_controller *ctrl, uint16_t pad, uint32_t *val);
};

/** @brief A pin controller. */
struct pbl_pinctrl_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_pinctrl_controller_ops *ops;
  /** Number of pads. */
  uint16_t num_pads;
};

/**
 * @brief Initializer for the struct pbl_pinctrl_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _ops Driver operations.
 * @param _num_pads Number of pads.
 */
#define PBL_PINCTRL_CONTROLLER_INIT(sym, _name, _ops, _num_pads)                             \
  {                                                                                          \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, nullptr, nullptr, nullptr, &pbl_pinctrl_class), \
    .ops = (_ops),                                                                           \
    .num_pads = (_num_pads),                                                                 \
  }

/** @brief A pin state. */
struct pbl_pinctrl_state {
  /** Controller of the pins. */
  const struct pbl_pinctrl_controller *ctrl;
  /** Pins, in the controller's encoding. */
  const uint32_t *pins;
  /** Number of pins. */
  uint8_t num_pins;
};

/**
 * @brief Define a pin state.
 *
 * @param sym Symbol of the state.
 * @param _ctrl Pin controller.
 * @param ... Pins, in the controller's encoding.
 */
#define PBL_PINCTRL_STATE_DEFINE(sym, _ctrl, ...)     \
  static const uint32_t sym##_pins[] = {__VA_ARGS__}; \
  static const struct pbl_pinctrl_state sym = {       \
    .ctrl = (_ctrl),                                  \
    .pins = sym##_pins,                               \
    .num_pins = ARRAY_LENGTH(sym##_pins),             \
  }

/** @brief State identifiers. */
enum pbl_pinctrl_state_id {
  /** Pins in use. */
  PBL_PINCTRL_STATE_DEFAULT,
  /** Pins of a powered-down peripheral. */
  PBL_PINCTRL_STATE_SLEEP,
  /** @cond INTERNAL_HIDDEN */
  PBL_PINCTRL_STATE_COUNT,
  /** @endcond */
};

/** @brief Pin states of a peripheral, as used by a consumer. */
struct pbl_pinctrl {
  /** States by identifier, NULL if the board gives none. */
  const struct pbl_pinctrl_state *states[PBL_PINCTRL_STATE_COUNT];
  /** Register base of the peripheral. */
  uintptr_t reg;
};

/**
 * @brief Initializer for a struct pbl_pinctrl.
 *
 * @param _default Default state.
 * @param _sleep Sleep state, or NULL.
 * @param _reg Register base of the peripheral.
 */
#define PBL_PINCTRL(_default, _sleep, _reg)                                                     \
  {                                                                                             \
    .states = {[PBL_PINCTRL_STATE_DEFAULT] = (_default), [PBL_PINCTRL_STATE_SLEEP] = (_sleep)}, \
    .reg = (_reg),                                                                              \
  }

/**
 * @brief Configure the pins of a state.
 *
 * @param pinctrl Pin states of the peripheral.
 * @param id State.
 * @return 0, @c -ENOENT if the board gives no such state, or another negative errno.
 */
int pbl_pinctrl_select_state(const struct pbl_pinctrl *pinctrl, enum pbl_pinctrl_state_id id);

/** @} */
