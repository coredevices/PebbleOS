/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_clk_nrf54l nRF54L
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller of the nRF54L clock control block.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_nrf54l;
/** @endcond */

/** @brief Clock controller. */
#define NRF54L_CLOCK (&pbl_clk_nrf54l)

/**
 * @name Clocks
 * @{
 */
/** HFCLK: from the HFXO crystal oscillator while enabled, the internal RC otherwise. */
#define NRF54L_CLK_HFCLK 0
/** LFCLK, from the LFXO crystal oscillator while enabled. */
#define NRF54L_CLK_LFCLK 1
/** XO24M, 24 MHz from the HFXO, on the nRF54LM20A. */
#define NRF54L_CLK_XO24M 2
/** @} */

/** @brief Clock control block. */
struct pbl_clk_nrf54l {
  /** Clock controller. */
  struct pbl_clk_controller ctrl;
  /** Registers. */
  uintptr_t base;
  /** Registers of the oscillators block. */
  uintptr_t oscillators;
  /** FICR trim of the HFXO load capacitors. */
  struct pbl_nvmem_cell hfxo_trim;
  /** FICR trim of the LFXO load capacitors. */
  struct pbl_nvmem_cell lfxo_trim;
  /** HFXO crystal. */
  struct pbl_clk hfxo;
  /** LFXO crystal. */
  struct pbl_clk lfxo;
  /** Internal load capacitance of the HFXO crystal, in fF, or 0 for none. */
  uint32_t hfxo_cap_ff;
  /** Internal load capacitance of the LFXO crystal, in fF, or 0 for none. */
  uint32_t lfxo_cap_ff;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller_ops pbl_clk_nrf54l_ops;
int pbl_clk_nrf54l_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the clock control block.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _oscillators Registers of the oscillators block.
 * @param _hfxo_trim FICR trim of the HFXO load capacitors, from PBL_NVMEM_CELL().
 * @param _lfxo_trim FICR trim of the LFXO load capacitors, from PBL_NVMEM_CELL().
 * @param _hfxo HFXO crystal, from PBL_CLK().
 * @param _lfxo LFXO crystal, from PBL_CLK().
 * @param _hfxo_cap_ff Internal load capacitance of the HFXO crystal, in fF, or 0 for none.
 * @param _lfxo_cap_ff Internal load capacitance of the LFXO crystal, in fF, or 0 for none.
 */
#define PBL_CLK_NRF54L_DEFINE(sym, _name, _base, _oscillators, _hfxo_trim, _lfxo_trim, _hfxo,   \
                              _lfxo, _hfxo_cap_ff, _lfxo_cap_ff)                                \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, 3);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_nrf54l, struct pbl_clk_controller, ctrl) {            \
    .ctrl =                                                                                     \
        PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_nrf54l_init, nullptr, &pbl_clk_nrf54l_ops), \
    .base = (_base), .oscillators = (_oscillators), .hfxo_trim = _hfxo_trim,                    \
    .lfxo_trim = _lfxo_trim, .hfxo = _hfxo, .lfxo = _lfxo, .hfxo_cap_ff = (_hfxo_cap_ff),       \
    .lfxo_cap_ff = (_lfxo_cap_ff),                                                              \
  }

/** @} */
