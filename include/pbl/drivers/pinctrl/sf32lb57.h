/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pinctrl/sf32lb57_pins.h>

/**
 * @defgroup drivers_pinctrl_sf32lb57 SF32LB57 pin controller
 * @ingroup drivers_pinctrl
 * @brief @ref drivers_pinctrl controller of the SF32LB57 HPSYS pads.
 *
 * A pin is a pad function from @ref sf32lb57_pins.h, such as @c SF32LB57_PA19_USART1_TXD, or'ed
 * with its pad options. Every option is written when the pin is configured, so a pin without
 * any gets no pull, a disabled input buffer, a fast slew rate and 2 mA drive:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_uart1_default, SF32LB57_PINCTRL,
 *                          SF32LB57_PA19_USART1_TXD | PBL_PINCTRL_SF32LB57_DRIVE_4MA,
 *                          SF32LB57_PA18_USART1_RXD | PBL_PINCTRL_SF32LB57_PULL_UP |
 *                              PBL_PINCTRL_SF32LB57_INPUT | PBL_PINCTRL_SF32LB57_DRIVE_4MA);
 * @endcode
 *
 * Any PA pad takes any of the signals of the FreeIO matrix, its function select alone routing
 * the signal.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_PINCTRL_SF32LB57_FSEL_MSK 0x000000FFU
#define PBL_PINCTRL_SF32LB57_PE       (1U << 8)
#define PBL_PINCTRL_SF32LB57_PS       (1U << 9)
#define PBL_PINCTRL_SF32LB57_IE       (1U << 10)
#define PBL_PINCTRL_SF32LB57_SR       (1U << 12)
#define PBL_PINCTRL_SF32LB57_DS_POS   13U
#define PBL_PINCTRL_SF32LB57_DS_MSK   0x00006000U
#define PBL_PINCTRL_SF32LB57_PORT_POS 16U
#define PBL_PINCTRL_SF32LB57_PORT_MSK 0x00030000U
#define PBL_PINCTRL_SF32LB57_PAD_POS  18U
#define PBL_PINCTRL_SF32LB57_PAD_MSK  0x00FC0000U

#define PBL_PINCTRL_SF32LB57_DS_IDX(idx) ((uint32_t)(idx) << PBL_PINCTRL_SF32LB57_DS_POS)
/** @endcond */

/** SA port. */
#define PBL_PINCTRL_SF32LB57_PORT_SA 0U
/** SB port. */
#define PBL_PINCTRL_SF32LB57_PORT_SB 1U
/** PA port. */
#define PBL_PINCTRL_SF32LB57_PORT_PA 2U

/**
 * @brief Pad function, for @ref sf32lb57_pins.h.
 *
 * @param port Port.
 * @param pad Pad.
 * @param fsel Function select.
 */
#define PBL_PINCTRL_SF32LB57_PIN(port, pad, fsel)      \
  (((uint32_t)(pad) << PBL_PINCTRL_SF32LB57_PAD_POS) | \
   ((uint32_t)(port) << PBL_PINCTRL_SF32LB57_PORT_POS) | (uint32_t)(fsel))

/**
 * @name Pad options
 * @{
 */
/** Pull-up. */
#define PBL_PINCTRL_SF32LB57_PULL_UP (PBL_PINCTRL_SF32LB57_PE | PBL_PINCTRL_SF32LB57_PS)
/** Pull-down. */
#define PBL_PINCTRL_SF32LB57_PULL_DOWN PBL_PINCTRL_SF32LB57_PE
/** Input buffer enabled. */
#define PBL_PINCTRL_SF32LB57_INPUT PBL_PINCTRL_SF32LB57_IE
/** Slow slew rate. */
#define PBL_PINCTRL_SF32LB57_SLEW_SLOW PBL_PINCTRL_SF32LB57_SR
/** 2 mA drive. */
#define PBL_PINCTRL_SF32LB57_DRIVE_2MA PBL_PINCTRL_SF32LB57_DS_IDX(0)
/** 4 mA drive. */
#define PBL_PINCTRL_SF32LB57_DRIVE_4MA PBL_PINCTRL_SF32LB57_DS_IDX(2)
/** 8 mA drive. */
#define PBL_PINCTRL_SF32LB57_DRIVE_8MA PBL_PINCTRL_SF32LB57_DS_IDX(1)
/** 12 mA drive. */
#define PBL_PINCTRL_SF32LB57_DRIVE_12MA PBL_PINCTRL_SF32LB57_DS_IDX(3)
/** @} */

/** @brief SF32LB57 pin controller. */
struct pbl_pinctrl_sf32lb57 {
  /** Pin controller. */
  struct pbl_pinctrl_controller ctrl;
  /** Pad registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pinctrl_controller pbl_pinctrl_sf32lb57_hpsys;
/** @endcond */

/** HPSYS pin controller. */
#define SF32LB57_PINCTRL (&pbl_pinctrl_sf32lb57_hpsys)

/** @} */
