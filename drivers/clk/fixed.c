/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/fixed.h>
#include <pbl/util/misc.h>

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  *rate_mhz = (uint64_t)container_of(ctrl, const struct pbl_clk_fixed, ctrl)->rate_hz * 1000U;

  return 0;
}

const struct pbl_clk_controller_ops pbl_clk_fixed_ops = {
  .get_rate = prv_get_rate,
};

#include PBL_DT_INSTANCES(fixed_clock)
