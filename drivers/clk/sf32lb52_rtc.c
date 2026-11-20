/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_rtc.h>

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb52_rtc *rtc =
      PBL_CONTAINER_OF(ctrl, const struct pbl_clk_sf32lb52_rtc, ctrl);

  *parent = (pbl_readl(rtc->base + SF32LB52_RTC_CR_OFFSET) & RTC_CR_LPCKSEL) != 0U ? rtc->lxt32
                                                                                   : rtc->lrc10;

  return 0;
}

const struct pbl_clk_controller_ops pbl_clk_sf32lb52_rtc_ops = {
  .get_parent = prv_get_parent,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_rtc_clk)
