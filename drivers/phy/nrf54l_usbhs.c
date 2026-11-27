/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/phy.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/work.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_usbhs.h>
#include <hardware/nrf54l_vregusb.h>

// The PHY starts within 45 us of its power-on reset, the controller within 1 us of its own
#define PHY_START_US  45U
#define CORE_START_US 1U

struct pbl_phy_nrf54l_usbhs_data {
  // VBUS, as VREGUSB last reported it
  bool vbus;
  struct pbl_work work;
  const struct pbl_phy *phy;
};

// The wrapper of the USBHS controller and its PHY, which powers and resets them, and VREGUSB,
// which senses VBUS
struct pbl_phy_nrf54l_usbhs {
  struct pbl_phy phy;
  uintptr_t base;
  struct pbl_clk clk;
  uintptr_t vregusb;
  pbl_irq_t vregusb_irq;
  struct pbl_phy_nrf54l_usbhs_data *data;
};

static const struct pbl_phy_nrf54l_usbhs *prv_usbhs(const struct pbl_phy *phy) {
  return PBL_CONTAINER_OF(phy, const struct pbl_phy_nrf54l_usbhs, phy);
}

static bool prv_event(const struct pbl_phy_nrf54l_usbhs *u, uint32_t event) {
  if (pbl_readl(u->vregusb + event) == 0U) {
    return false;
  }
  pbl_writel(0U, u->vregusb + event);
  (void)pbl_readl(u->vregusb + event);

  return true;
}

static bool prv_vbus_status(const struct pbl_phy_nrf54l_usbhs *u) {
  return (pbl_readl(u->vregusb + NRF54L_VREGUSB_UNDOC_STATUS_OFFSET) & VREGUSB_STATUS_VBUS) != 0U;
}

// The PHY held at device without VBUS, so that the pull-up on D+ stays off
static void prv_hold(const struct pbl_phy_nrf54l_usbhs *u) {
  pbl_writel(USBHS_PHY_DEVICE, u->base + NRF54L_USBHS_PHY_OVERRIDEVALUES_OFFSET);
  pbl_writel(USBHS_PHY_DEVICE | USBHS_PHY_VBUSVALID,
             u->base + NRF54L_USBHS_PHY_INPUTOVERRIDE_OFFSET);
}

static void prv_work(struct pbl_work *work) {
  struct pbl_phy_nrf54l_usbhs_data *data =
      PBL_CONTAINER_OF(work, struct pbl_phy_nrf54l_usbhs_data, work);

  pbl_phy_report_vbus_changed(data->phy);
}

// VBUS from the event, as the status may lag it, or from the status when both events are pending,
// whose order is lost
static void prv_vregusb_isr(const struct pbl_phy_nrf54l_usbhs *u) {
  bool detected = prv_event(u, NRF54L_VREGUSB_EVENTS_VBUSDETECTED_OFFSET);
  bool removed = prv_event(u, NRF54L_VREGUSB_EVENTS_VBUSREMOVED_OFFSET);

  if (detected && removed) {
    u->data->vbus = prv_vbus_status(u);
  } else if (detected || removed) {
    u->data->vbus = detected;
  }

  (void)pbl_work_submit(&pbl_sys_workq_high, &u->data->work);
}

// VREGUSB started: VBUS from its events, and from its status for VBUS present before, which
// raises no event
static int prv_init(const struct pbl_phy *phy) {
  const struct pbl_phy_nrf54l_usbhs *u = prv_usbhs(phy);
  int ret;

  ret = pbl_device_init(&u->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  u->data->phy = phy;
  pbl_work_init(&u->data->work, prv_work);

  prv_hold(u);
  pbl_writel(0U, u->base + NRF54L_USBHS_ENABLE_OFFSET);

  (void)prv_event(u, NRF54L_VREGUSB_EVENTS_VBUSDETECTED_OFFSET);
  (void)prv_event(u, NRF54L_VREGUSB_EVENTS_VBUSREMOVED_OFFSET);
  pbl_writel(VREGUSB_INT_VBUSDETECTED | VREGUSB_INT_VBUSREMOVED,
             u->vregusb + NRF54L_VREGUSB_INTENSET_OFFSET);
  pbl_writel(1U, u->vregusb + NRF54L_VREGUSB_TASKS_START_OFFSET);

  pbl_irq_lock();
  u->data->vbus = prv_vbus_status(u);
  pbl_irq_unlock();
  pbl_irq_enable(u->vregusb_irq);

  return 0;
}

// The controller and the PHY powered and out of reset; the controller then drives no line, and
// the PHY is released, the pull-up on D+ up to the controller
static int prv_power_on(const struct pbl_phy *phy) {
  const struct pbl_phy_nrf54l_usbhs *u = prv_usbhs(phy);
  int ret;

  ret = pbl_clk_enable(&u->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(USBHS_ENABLE_CORE, u->base + NRF54L_USBHS_ENABLE_OFFSET);
  prv_hold(u);
  pbl_writel(USBHS_ENABLE_CORE | USBHS_ENABLE_PHY, u->base + NRF54L_USBHS_ENABLE_OFFSET);
  pbl_busy_wait_us(PHY_START_US);
  pbl_writel(1U, u->base + NRF54L_USBHS_TASKS_START_OFFSET);
  pbl_busy_wait_us(CORE_START_US);
  pbl_writel(USBHS_PHY_DEVICE, u->base + NRF54L_USBHS_PHY_INPUTOVERRIDE_OFFSET);

  return 0;
}

static int prv_power_off(const struct pbl_phy *phy) {
  const struct pbl_phy_nrf54l_usbhs *u = prv_usbhs(phy);

  prv_hold(u);
  pbl_writel(0U, u->base + NRF54L_USBHS_ENABLE_OFFSET);

  return pbl_clk_disable(&u->clk);
}

static int prv_vbus_present(const struct pbl_phy *phy, bool *present) {
  *present = prv_usbhs(phy)->data->vbus;

  return 0;
}

static const struct pbl_phy_ops s_ops = {
  .init = prv_init,
  .power_on = prv_power_on,
  .power_off = prv_power_off,
  .vbus_present = prv_vbus_present,
};

#define PBL_PHY_NRF54L_USBHS_DEFINE(sym, _name, _base, _clk, _vregusb, _vregusb_irq)   \
  PBL_PHY_STATE_DEFINE(sym);                                                           \
  static struct pbl_phy_nrf54l_usbhs_data sym##_usbhs_data;                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_phy_nrf54l_usbhs, struct pbl_phy, phy){          \
    .phy = PBL_PHY_INIT(sym, _name, nullptr, &s_ops),                                  \
    .base = (_base),                                                                   \
    .clk = _clk,                                                                       \
    .vregusb = (_vregusb),                                                             \
    .vregusb_irq = (_vregusb_irq),                                                     \
    .data = &sym##_usbhs_data,                                                         \
  };                                                                                   \
  PBL_IRQ_CONNECT_NUM(_vregusb_irq, CONFIG_PHY_NRF54L_USBHS_IRQ_PRIO, prv_vregusb_isr, \
                      &sym##_inst, 0)

#include PBL_DT_INSTANCES(nordic_nrf54lm20a_usbhs_phy)
