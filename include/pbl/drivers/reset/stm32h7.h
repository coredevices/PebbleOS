/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/reset.h>

/**
 * @defgroup reset_stm32h7 STM32H7 RCC
 * @ingroup drivers_reset
 * @brief Reset controller of the STM32H7 RCC.
 *
 * Its lines are numbered as the binding does, dt-bindings/mfd/stm32h7-rcc.h: the bit of a
 * peripheral in the reset registers, eight per byte of offset in the RCC.
 * @{
 */

extern const struct pbl_reset_controller pbl_reset_stm32h7_rcc;

/** The RCC, as a reset controller. */
#define STM32H7_RCC_RESET (&pbl_reset_stm32h7_rcc)

/** @brief RCC reset controller. */
struct pbl_reset_stm32h7 {
  /** Reset controller. */
  struct pbl_reset_controller ctrl;
  /** Registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_reset_controller_ops pbl_reset_stm32h7_ops;
/** @endcond */

/** Number of reset lines of the RCC: up to the last bit of APB4RSTR. */
#define PBL_RESET_STM32H7_NUM_LINES (0xa0U * 8U)

/**
 * @brief Define the reset controller of the RCC.
 *
 * @param sym Symbol of the controller.
 * @param _name Name of the RCC; the controller is named after it.
 * @param _base RCC registers.
 */
#define PBL_RESET_STM32H7_DEFINE(sym, _name, _base)                                         \
  PBL_RESET_CONTROLLER_STATE_DEFINE(sym);                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_reset_stm32h7, struct pbl_reset_controller, ctrl) {   \
    .ctrl = PBL_RESET_CONTROLLER_INIT(sym, _name "_reset", nullptr, nullptr,                \
                                      &pbl_reset_stm32h7_ops, PBL_RESET_STM32H7_NUM_LINES), \
    .base = (_base),                                                                        \
  }

/** @} */
