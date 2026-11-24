/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/sf32lb.h>

/**
 * @defgroup drivers_clk_sf32lb_lpsys_rcc SF32LB LPSYS RCC clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller for the LPSYS RCC of the SF32LB SoCs.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_clk_sf32lb_lpsys_rcc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hrc48;
  struct pbl_clk hxt48;
  const struct pbl_clk_assigned *assigned;
};

extern const struct pbl_clk_controller_ops pbl_clk_sf32lb_lpsys_rcc_ops;
int pbl_clk_sf32lb_lpsys_rcc_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the LPSYS RCC clock controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base LPSYS RCC registers.
 * @param _hrc48 HRC48 oscillator, from PBL_CLK().
 * @param _hxt48 HXT48 crystal, from PBL_CLK().
 * @param ... Assigned clocks, from PBL_CLK_ASSIGNED().
 */
#define PBL_CLK_SF32LB_LPSYS_RCC_DEFINE(sym, _name, _base, _hrc48, _hxt48, ...)                \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_LPSYS_NUM_CLKS);                               \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb_lpsys_rcc, struct pbl_clk_controller, ctrl) { \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_sf32lb_lpsys_rcc_init, nullptr,        \
                                    &pbl_clk_sf32lb_lpsys_rcc_ops),                            \
    .base = (_base), .hrc48 = _hrc48, .hxt48 = _hxt48, .assigned = sym##_assigned,             \
  }

/** @} */
