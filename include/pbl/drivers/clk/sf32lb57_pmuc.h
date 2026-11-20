/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/sf32lb.h>

/**
 * @defgroup drivers_clk_sf32lb57_pmuc SF32LB57 PMUC clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller for the oscillators of the SF32LB57 PMUC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_clk_sf32lb57_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  // HPSYS RCC, whose counter measures LRC10
  uintptr_t rcc;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
  const struct pbl_clk_assigned *assigned;
};

extern const struct pbl_clk_controller_ops pbl_clk_sf32lb57_pmuc_ops;
int pbl_clk_sf32lb57_pmuc_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the SF32LB57 PMUC clock controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base PMUC registers.
 * @param _rcc HPSYS RCC registers, whose counter measures LRC10.
 * @param _hxt48 HXT48 crystal, from PBL_CLK().
 * @param _lxt32 LXT32 crystal, from PBL_CLK(), or {} without one.
 * @param ... Assigned clocks, from PBL_CLK_ASSIGNED().
 */
#define PBL_CLK_SF32LB57_PMUC_DEFINE(sym, _name, _base, _rcc, _hxt48, _lxt32, ...)                \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB57_PMUC_NUM_CLKS);                                   \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb57_pmuc, struct pbl_clk_controller, ctrl) {       \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_sf32lb57_pmuc_init, nullptr,              \
                                    &pbl_clk_sf32lb57_pmuc_ops),                                  \
    .base = (_base), .rcc = (_rcc), .hxt48 = _hxt48, .lxt32 = _lxt32, .assigned = sym##_assigned, \
  }

/** @} */
