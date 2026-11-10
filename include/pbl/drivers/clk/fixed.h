/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_fixed Fixed-rate clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller of one clock that always runs at the same rate, as a
 * crystal.
 *
 * @code{.c}
 * PBL_CLK_FIXED_DEFINE(s_xtal48m, "xtal48m", 48000000);
 * @endcode
 * @{
 */

/** @brief Fixed-rate clock. */
struct pbl_clk_fixed {
  /** Clock controller. */
  struct pbl_clk_controller ctrl;
  /** Rate in Hz. */
  uint32_t rate_hz;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller_ops pbl_clk_fixed_ops;
/** @endcond */

/**
 * @brief Define a fixed-rate clock, clock 0 of its controller.
 *
 * @param sym Symbol of the clock controller.
 * @param _name Name.
 * @param _rate_hz Rate in Hz.
 */
#define PBL_CLK_FIXED_DEFINE(sym, _name, _rate_hz)                                     \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, 1);                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_fixed, struct pbl_clk_controller, ctrl) {    \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &pbl_clk_fixed_ops), \
    .rate_hz = (_rate_hz),                                                             \
  }

/** @} */
