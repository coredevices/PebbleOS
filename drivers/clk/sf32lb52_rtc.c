/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_rtc.h>

struct pbl_clk_sf32lb52_rtc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk lrc10;
  struct pbl_clk lxt32;
};

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb52_rtc *rtc =
      container_of(ctrl, const struct pbl_clk_sf32lb52_rtc, ctrl);

  *parent = (pbl_readl(rtc->base + SF32LB52_RTC_CR_OFFSET) & RTC_CR_LPCKSEL) != 0U ? rtc->lxt32
                                                                                   : rtc->lrc10;

  return 0;
}

static const struct pbl_clk_controller_ops s_ops = {
  .get_parent = prv_get_parent,
};

#define PBL_CLK_SF32LB52_RTC_DEFINE(sym, _name, _base, _lrc10, _lxt32)                      \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, 1);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb52_rtc, struct pbl_clk_controller, ctrl) {  \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &s_ops), .base = (_base), \
    .lrc10 = _lrc10, .lxt32 = _lxt32,                                                       \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_rtc)
