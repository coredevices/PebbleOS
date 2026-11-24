/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/sf32lb.h>

/**
 * @defgroup drivers_clk_sf32lb52_pmuc SF32LB52 PMUC clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller for the oscillators of the SF32LB52 PMUC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_clk_sf32lb52_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  uintptr_t rcc;
  // LRC10 calibration: the counter of the Bluetooth controller, LPSYS and the lock shared with
  // the LCPU
  uintptr_t bt_mac;
  uintptr_t lpsys_aon;
  uintptr_t lpsys_rcc;
  uintptr_t lock;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
  const struct pbl_clk_assigned *assigned;
};

extern const struct pbl_clk_controller_ops pbl_clk_sf32lb52_pmuc_ops;
int pbl_clk_sf32lb52_pmuc_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the SF32LB52 PMUC clock controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base PMUC registers.
 * @param _rcc HPSYS RCC registers.
 * @param _bt_mac Bluetooth controller registers, whose counter calibrates LRC10.
 * @param _lpsys_aon LPSYS_AON registers.
 * @param _lpsys_rcc LPSYS RCC registers.
 * @param _lock Hardware lock shared with the LCPU.
 * @param _hxt48 HXT48 crystal, from PBL_CLK().
 * @param _lxt32 LXT32 crystal, from PBL_CLK().
 * @param ... Assigned clocks, from PBL_CLK_ASSIGNED().
 */
#define PBL_CLK_SF32LB52_PMUC_DEFINE(sym, _name, _base, _rcc, _bt_mac, _lpsys_aon, _lpsys_rcc, \
                                     _lock, _hxt48, _lxt32, ...)                               \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_PMUC_NUM_CLKS);                                \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb52_pmuc, struct pbl_clk_controller, ctrl) {    \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_sf32lb52_pmuc_init, nullptr,           \
                                    &pbl_clk_sf32lb52_pmuc_ops),                               \
    .base = (_base), .rcc = (_rcc), .bt_mac = (_bt_mac), .lpsys_aon = (_lpsys_aon),            \
    .lpsys_rcc = (_lpsys_rcc), .lock = (_lock), .hxt48 = _hxt48, .lxt32 = _lxt32,              \
    .assigned = sym##_assigned,                                                                \
  }

/** @} */
