/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/phy.h>
#include <pbl/kernel/assert.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_phy_class, "phy");

static void prv_lock(const struct pbl_phy *phy) {
  PBL_ASSERTN(pbl_device_is_ready(&phy->dev));
  pbl_mutex_lock(&phy->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_phy *phy) {
  pbl_mutex_unlock(&phy->state->lock);
}

int pbl_phy_dev_init(const struct pbl_device *dev) {
  const struct pbl_phy *phy = PBL_CONTAINER_OF(dev, const struct pbl_phy, dev);
  int ret;

  if (phy->ops->init == nullptr) {
    return 0;
  }

  pbl_mutex_lock(&phy->state->lock, PBL_FOREVER);
  ret = phy->ops->init(phy);
  pbl_mutex_unlock(&phy->state->lock);

  return ret;
}

int pbl_phy_power_on(const struct pbl_phy *phy) {
  int ret;

  prv_lock(phy);
  ret = phy->ops->power_on(phy);
  prv_unlock(phy);

  return ret;
}

int pbl_phy_power_off(const struct pbl_phy *phy) {
  int ret;

  prv_lock(phy);
  ret = phy->ops->power_off(phy);
  prv_unlock(phy);

  return ret;
}

int pbl_phy_vbus_present(const struct pbl_phy *phy, bool *present) {
  int ret;

  if (phy->ops->vbus_present == nullptr) {
    return -ENOTSUP;
  }

  prv_lock(phy);
  ret = phy->ops->vbus_present(phy, present);
  prv_unlock(phy);

  return ret;
}

void pbl_phy_add_listener(const struct pbl_phy *phy, struct pbl_phy_listener *listener) {
  prv_lock(phy);
  listener->next = phy->state->listeners;
  phy->state->listeners = listener;
  prv_unlock(phy);
}

// The listeners query the PHY, so they run unlocked; they are only ever added
void pbl_phy_report_vbus_changed(const struct pbl_phy *phy) {
  struct pbl_phy_listener *l;

  prv_lock(phy);
  l = phy->state->listeners;
  prv_unlock(phy);

  for (; l != nullptr; l = l->next) {
    l->vbus_changed(phy, l);
  }
}
