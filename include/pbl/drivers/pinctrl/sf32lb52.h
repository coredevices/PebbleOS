/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pinctrl/sf32lb52_pins.h>

/**
 * @defgroup drivers_pinctrl_sf32lb52 SF32LB52 pin controller
 * @ingroup drivers_pinctrl
 * @brief @ref drivers_pinctrl controller of the SF32LB52 HPSYS pads.
 *
 * A pin is a pad function from @ref sf32lb52_pins.h, such as @c SF32LB52_PA19_USART1_TXD, or'ed
 * with its pad options. Every option is written when the pin is configured, so a pin without
 * any gets no pull, a disabled input buffer, a fast slew rate and 2 mA drive:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_uart1_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA19_USART1_TXD | PBL_PINCTRL_SF32LB52_DRIVE_4MA,
 *                          SF32LB52_PA18_USART1_RXD | PBL_PINCTRL_SF32LB52_PULL_UP |
 *                              PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_4MA);
 * @endcode
 *
 * Crossbar functions (I2C, USART and timers) also route the pad in the HPSYS_CFG PINR
 * registers, and the backup outputs of PA24-PA27 (@c SF32LB52_PA24_PBR_LPTIM2_OUT, ...) are
 * driven from the RTC block. Pulls of the always-on and backup pads are mirrored there too.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_PINCTRL_SF32LB52_FSEL_MSK  0x0000000FU
#define PBL_PINCTRL_SF32LB52_PE        (1U << 4)
#define PBL_PINCTRL_SF32LB52_PS        (1U << 5)
#define PBL_PINCTRL_SF32LB52_IE        (1U << 6)
#define PBL_PINCTRL_SF32LB52_SR        (1U << 8)
#define PBL_PINCTRL_SF32LB52_DS_POS    9U
#define PBL_PINCTRL_SF32LB52_DS_MSK    0x00000E00U
#define PBL_PINCTRL_SF32LB52_PORT_POS  12U
#define PBL_PINCTRL_SF32LB52_PORT_MSK  0x00003000U
#define PBL_PINCTRL_SF32LB52_PAD_POS   14U
#define PBL_PINCTRL_SF32LB52_PAD_MSK   0x003FC000U
#define PBL_PINCTRL_SF32LB52_FIELD_POS 22U
#define PBL_PINCTRL_SF32LB52_FIELD_MSK 0x00C00000U
#define PBL_PINCTRL_SF32LB52_PINR_POS  24U
#define PBL_PINCTRL_SF32LB52_PINR_MSK  0xFF000000U

/* PINR offset of the PA24-PA27 backup (PBR) outputs, driven from the RTC block */
#define PBL_PINCTRL_SF32LB52_PINR_PBR 0xFFU

#define PBL_PINCTRL_SF32LB52_DS_IDX(idx) ((uint32_t)(idx) << PBL_PINCTRL_SF32LB52_DS_POS)
/** @endcond */

/** SA port. */
#define PBL_PINCTRL_SF32LB52_PORT_SA 0U
/** PA port. */
#define PBL_PINCTRL_SF32LB52_PORT_PA 1U

/**
 * @brief Pad function, for @ref sf32lb52_pins.h.
 *
 * @param port Port.
 * @param pad Pad.
 * @param fsel Function select.
 * @param pinr HPSYS_CFG PINR register offset of a crossbar function, 0 if none.
 * @param field PINR field (signal) of a crossbar function.
 */
#define PBL_PINCTRL_SF32LB52_PIN(port, pad, fsel, pinr, field) \
  (((uint32_t)(pinr) << PBL_PINCTRL_SF32LB52_PINR_POS) |       \
   ((uint32_t)(field) << PBL_PINCTRL_SF32LB52_FIELD_POS) |     \
   ((uint32_t)(pad) << PBL_PINCTRL_SF32LB52_PAD_POS) |         \
   ((uint32_t)(port) << PBL_PINCTRL_SF32LB52_PORT_POS) | (uint32_t)(fsel))

/**
 * @name Pad options
 * @{
 */
/** Pull-up. */
#define PBL_PINCTRL_SF32LB52_PULL_UP (PBL_PINCTRL_SF32LB52_PE | PBL_PINCTRL_SF32LB52_PS)
/** Pull-down. */
#define PBL_PINCTRL_SF32LB52_PULL_DOWN PBL_PINCTRL_SF32LB52_PE
/** Input buffer enabled. */
#define PBL_PINCTRL_SF32LB52_INPUT PBL_PINCTRL_SF32LB52_IE
/** Slow slew rate, ignored on PA39-PA42. */
#define PBL_PINCTRL_SF32LB52_SLEW_SLOW PBL_PINCTRL_SF32LB52_SR
/** 2 mA drive. */
#define PBL_PINCTRL_SF32LB52_DRIVE_2MA PBL_PINCTRL_SF32LB52_DS_IDX(0)
/** 4 mA drive. */
#define PBL_PINCTRL_SF32LB52_DRIVE_4MA PBL_PINCTRL_SF32LB52_DS_IDX(2)
/** 8 mA drive, not on PA39-PA42. */
#define PBL_PINCTRL_SF32LB52_DRIVE_8MA PBL_PINCTRL_SF32LB52_DS_IDX(1)
/** 12 mA drive, not on PA39-PA42. */
#define PBL_PINCTRL_SF32LB52_DRIVE_12MA PBL_PINCTRL_SF32LB52_DS_IDX(3)
/** 20 mA drive, PA39-PA42 only. */
#define PBL_PINCTRL_SF32LB52_DRIVE_20MA PBL_PINCTRL_SF32LB52_DS_IDX(4)
/** @} */

/** @brief SF32LB52 pin controller. */
struct pbl_pinctrl_sf32lb52 {
  /** Pin controller. */
  struct pbl_pinctrl_controller ctrl;
  /** SA pad registers. */
  uintptr_t pad_sa;
  /** PA pad registers. */
  uintptr_t pad_pa;
  /** HPSYS_CFG registers, holding the PINR crossbar. */
  uintptr_t cfg;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pinctrl_sf32lb52 pbl_pinctrl_sf32lb52_hpsys;

/* Set the @p mask bits of the pad of @p pin to its value, for the GPIO driver */
int pbl_pinctrl_sf32lb52_modify(uint32_t pin, uint32_t mask);
/** @endcond */

/** HPSYS pin controller. */
#define SF32LB52_PINCTRL (&pbl_pinctrl_sf32lb52_hpsys.ctrl)

/** @} */
