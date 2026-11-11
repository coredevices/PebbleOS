/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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

/** @} */
