/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_rcc.h>

#define RCC_NUM_LINES 64U

struct pbl_reset_sf32lb {
  struct pbl_reset_controller ctrl;
  uintptr_t base;
};

static inline uintptr_t prv_reg(const struct pbl_reset_controller *ctrl, uint32_t offset,
                                uint32_t id) {
  return container_of(ctrl, const struct pbl_reset_sf32lb, ctrl)->base + offset + ((id >> 5) << 2);
}

static int prv_reset_assert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(0U, PBL_BIT(id & 31U), prv_reg(ctrl, SF32LB52_HPSYS_RCC_RSTR1_OFFSET, id));

  return 0;
}

static int prv_reset_deassert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(PBL_BIT(id & 31U), 0U, prv_reg(ctrl, SF32LB52_HPSYS_RCC_RSTR1_OFFSET, id));

  return 0;
}

static const struct pbl_reset_controller_ops s_reset_ops = {
  .assert = prv_reset_assert,
  .deassert = prv_reset_deassert,
};

#define PBL_RESET_SF32LB_DEFINE(sym, _name, _base)                                         \
  PBL_RESET_CONTROLLER_STATE_DEFINE(sym);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_reset_sf32lb, struct pbl_reset_controller, ctrl) {   \
    .ctrl = PBL_RESET_CONTROLLER_INIT(sym, _name "_reset", nullptr, nullptr, &s_reset_ops, \
                                      RCC_NUM_LINES),                                      \
    .base = (_base),                                                                       \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_hpsys_rcc_reset)
#include PBL_DT_INSTANCES(sifli_sf32lb57_hpsys_rcc_reset)
