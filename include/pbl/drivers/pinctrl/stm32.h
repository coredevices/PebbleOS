/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/hwspinlock.h>
#include <pbl/drivers/pinctrl.h>

/**
 * @defgroup drivers_pinctrl_stm32 STM32 pin controller
 * @ingroup drivers_pinctrl
 * @brief @ref drivers_pinctrl controller of the STM32 GPIO ports.
 *
 * A pin is a pinmux cell of the upstream binding, STM32_PINMUX(port, line, mode), or'ed with its
 * options. Every option is written when the pin is configured, so a pin without any gets no
 * pull, a push-pull output and the lowest speed. The controller ungates the clock of a port the
 * first time it configures one of its pins:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_usart1_default, STM32_PINCTRL,
 *                          PBL_PINCTRL_STM32_PINMUX(STM32_PINMUX('A', 9, AF7)),
 *                          PBL_PINCTRL_STM32_PINMUX(STM32_PINMUX('A', 10, AF7)) |
 *                              PBL_PINCTRL_STM32_PULL_UP);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_PINCTRL_STM32_MODE_MSK  0x000000FFU
#define PBL_PINCTRL_STM32_LINE_MSK  0x00000F00U
#define PBL_PINCTRL_STM32_PORT_MSK  0x0000F000U
#define PBL_PINCTRL_STM32_PUPD_MSK  0x00030000U
#define PBL_PINCTRL_STM32_OD        (1U << 18)
#define PBL_PINCTRL_STM32_SPEED_MSK 0x00600000U
/** @endcond */

/** @brief A pinmux cell, the pin number and mode of STM32_PINMUX(). */
#define PBL_PINCTRL_STM32_PINMUX(cell) ((uint32_t)(cell))

/**
 * @name Pin options
 * @{
 */
/** No pull. */
#define PBL_PINCTRL_STM32_NO_PULL 0U
/** Pull-up. */
#define PBL_PINCTRL_STM32_PULL_UP (1U << 16)
/** Pull-down. */
#define PBL_PINCTRL_STM32_PULL_DOWN (2U << 16)
/** Push-pull output. */
#define PBL_PINCTRL_STM32_PUSH_PULL 0U
/** Open-drain output. */
#define PBL_PINCTRL_STM32_OPEN_DRAIN PBL_PINCTRL_STM32_OD
/** Output speed, 0 (low) to 3 (very high). */
#define PBL_PINCTRL_STM32_SPEED(n) ((uint32_t)(n) << 21)
/** @} */

/** @brief Number of GPIO ports of the controller, A to K. */
#define PBL_PINCTRL_STM32_NUM_PORTS 11U

/** @brief STM32 pin controller. */
struct pbl_pinctrl_stm32 {
  /** Pin controller. */
  struct pbl_pinctrl_controller ctrl;
  /** Registers of port A, the others following at 1 KiB steps. */
  uintptr_t base;
  /** Lock of the port registers, shared with the other cores. */
  struct pbl_hwspinlock hwlock;
  /** Clocks of the ports. */
  struct pbl_clk clks[PBL_PINCTRL_STM32_NUM_PORTS];
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pinctrl_controller pbl_pinctrl_stm32;
extern const struct pbl_pinctrl_controller_ops pbl_pinctrl_stm32_ops;
/** @endcond */

/** Pin controller. */
#define STM32_PINCTRL (&pbl_pinctrl_stm32)

/**
 * @brief Define the pin controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers of port A.
 * @param _hwlock Lock of the port registers, from PBL_HWSPINLOCK(), or PBL_HWSPINLOCK_NONE.
 * @param ... Clocks of ports A to K, from PBL_CLK().
 */
#define PBL_PINCTRL_STM32_DEFINE(sym, _name, _base, _hwlock, ...)                           \
  PBL_DEVICE_STATE_DEFINE(sym);                                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pinctrl_stm32, struct pbl_pinctrl_controller, ctrl) { \
    .ctrl = PBL_PINCTRL_CONTROLLER_INIT(sym, _name, nullptr, &pbl_pinctrl_stm32_ops,        \
                                        PBL_PINCTRL_STM32_NUM_PORTS * 16U),                 \
    .base = (_base), .hwlock = _hwlock, .clks = {__VA_ARGS__},                              \
  }

/** @} */
