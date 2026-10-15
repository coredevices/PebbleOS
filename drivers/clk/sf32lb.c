/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <hardware/sf32lb52_hpsys_rcc.h>

#define RCC_NUM_LINES 64U

static inline uintptr_t prv_reg(uint32_t offset, uint32_t id) {
  return SF32LB52_HPSYS_RCC_BASE + offset + ((id >> 5) << 2);
}

static int prv_clk_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  pbl_modifyl(0U, PBL_BIT(id & 31U), prv_reg(SF32LB52_HPSYS_RCC_ENR1_OFFSET, id));

  return 0;
}

static int prv_clk_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  pbl_modifyl(PBL_BIT(id & 31U), 0U, prv_reg(SF32LB52_HPSYS_RCC_ENR1_OFFSET, id));

  return 0;
}

static const struct pbl_clk_controller_ops s_clk_ops = {
  .enable = prv_clk_enable,
  .disable = prv_clk_disable,
};

PBL_CLK_CONTROLLER_STATE_DEFINE(pbl_clk_sf32lb_hpsys_rcc, RCC_NUM_LINES);
const struct pbl_clk_controller pbl_clk_sf32lb_hpsys_rcc =
    PBL_CLK_CONTROLLER_INIT(pbl_clk_sf32lb_hpsys_rcc, "hpsys_rcc", nullptr, nullptr, &s_clk_ops);
PBL_DEVICE_REGISTER(pbl_clk_sf32lb_hpsys_rcc, &pbl_clk_sf32lb_hpsys_rcc.dev);
