/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <hardware/sf32lb52_hpsys_rcc.h>

#define RCC_NUM_LINES 64U

static inline uintptr_t prv_reg(uint32_t offset, uint32_t id) {
  return SF32LB52_HPSYS_RCC_BASE + offset + ((id >> 5) << 2);
}

static int prv_reset_assert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(0U, PBL_BIT(id & 31U), prv_reg(SF32LB52_HPSYS_RCC_RSTR1_OFFSET, id));

  return 0;
}

static int prv_reset_deassert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(PBL_BIT(id & 31U), 0U, prv_reg(SF32LB52_HPSYS_RCC_RSTR1_OFFSET, id));

  return 0;
}

static const struct pbl_reset_controller_ops s_reset_ops = {
  .assert = prv_reset_assert,
  .deassert = prv_reset_deassert,
};

PBL_RESET_CONTROLLER_STATE_DEFINE(pbl_reset_sf32lb_hpsys_rcc);
const struct pbl_reset_controller pbl_reset_sf32lb_hpsys_rcc = PBL_RESET_CONTROLLER_INIT(
    pbl_reset_sf32lb_hpsys_rcc, "hpsys_rcc_reset", nullptr, nullptr, &s_reset_ops, RCC_NUM_LINES);
PBL_DEVICE_REGISTER(pbl_reset_sf32lb_hpsys_rcc, &pbl_reset_sf32lb_hpsys_rcc.dev);
