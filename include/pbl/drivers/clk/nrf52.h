/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_nrf52 nRF52
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller of the nRF52 clock control block.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_nrf52;
/** @endcond */

/** @brief Clock controller. */
#define NRF52_CLOCK (&pbl_clk_nrf52)

/**
 * @name Clocks
 * @{
 */
/** HFCLK, 64 MHz: from the HFXO crystal oscillator while enabled, the internal RC otherwise. */
#define NRF52_CLK_HFCLK 0
/** LFCLK, from the LFXO crystal oscillator while enabled. */
#define NRF52_CLK_LFCLK 1
/** @} */

/** @brief Clock control block. */
struct pbl_clk_nrf52 {
  /** Clock controller. */
  struct pbl_clk_controller ctrl;
  /** Registers. */
  uintptr_t base;
  /** HFXO crystal. */
  struct pbl_clk hfxo;
  /** LFXO crystal. */
  struct pbl_clk lfxo;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller_ops pbl_clk_nrf52_ops;
/** @endcond */

/**
 * @brief Define the clock control block.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _hfxo HFXO crystal, from PBL_CLK().
 * @param _lfxo LFXO crystal, from PBL_CLK().
 */
#define PBL_CLK_NRF52_DEFINE(sym, _name, _base, _hfxo, _lfxo)                          \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, 2);                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_nrf52, struct pbl_clk_controller, ctrl) {    \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &pbl_clk_nrf52_ops), \
    .base = (_base), .hfxo = _hfxo, .lfxo = _lfxo,                                     \
  }

/** @} */
