/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/phy.h>
#include <pbl/util/misc.h>

// A USB transceiver software has nothing to set up in, as a ULPI PHY, which only needs its clock
struct pbl_phy_usb_nop {
  struct pbl_phy phy;
  struct pbl_clk clk;
};

static const struct pbl_phy_usb_nop *prv_nop(const struct pbl_phy *phy) {
  return PBL_CONTAINER_OF(phy, const struct pbl_phy_usb_nop, phy);
}

static int prv_init(const struct pbl_phy *phy) {
  const struct pbl_phy_usb_nop *nop = prv_nop(phy);

  if (nop->clk.ctrl == nullptr) {
    return 0;
  }

  return pbl_device_init(&nop->clk.ctrl->dev);
}

static int prv_power_on(const struct pbl_phy *phy) {
  const struct pbl_phy_usb_nop *nop = prv_nop(phy);

  return nop->clk.ctrl == nullptr ? 0 : pbl_clk_enable(&nop->clk);
}

static int prv_power_off(const struct pbl_phy *phy) {
  const struct pbl_phy_usb_nop *nop = prv_nop(phy);

  return nop->clk.ctrl == nullptr ? 0 : pbl_clk_disable(&nop->clk);
}

static const struct pbl_phy_ops s_ops = {
  .init = prv_init,
  .power_on = prv_power_on,
  .power_off = prv_power_off,
};

#define PBL_PHY_USB_NOP_DEFINE(sym, _name, _clk)                          \
  PBL_PHY_STATE_DEFINE(sym);                                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_phy_usb_nop, struct pbl_phy, phy) { \
    .phy = PBL_PHY_INIT(sym, _name, nullptr, &s_ops), .clk = _clk,        \
  }

#include PBL_DT_INSTANCES(usb_nop_xceiv)
