/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/reset/stm32h7.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

static inline uintptr_t prv_reg(const struct pbl_reset_controller *ctrl, uint32_t id) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_reset_stm32h7, ctrl)->base + ((id / 32U) * 4U);
}

static int prv_reset_assert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(0U, PBL_BIT(id % 32U), prv_reg(ctrl, id));

  return 0;
}

static int prv_reset_deassert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  pbl_modifyl(PBL_BIT(id % 32U), 0U, prv_reg(ctrl, id));

  return 0;
}

const struct pbl_reset_controller_ops pbl_reset_stm32h7_ops = {
  .assert = prv_reset_assert,
  .deassert = prv_reset_deassert,
};

#include PBL_DT_INSTANCES(st_stm32h743_rcc_reset)
