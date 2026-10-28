/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/panel.h>

PBL_DEVICE_CLASS_DEFINE(pbl_panel_class, "panel");

int pbl_panel_prepare(const struct pbl_panel *panel) {
  if (!pbl_device_is_ready(&panel->dev)) {
    return -ENODEV;
  }

  return panel->ops->prepare(panel);
}

int pbl_panel_enable(const struct pbl_panel *panel) {
  if (!pbl_device_is_ready(&panel->dev)) {
    return -ENODEV;
  }

  return panel->ops->enable(panel);
}

int pbl_panel_disable(const struct pbl_panel *panel) {
  if (!pbl_device_is_ready(&panel->dev)) {
    return -ENODEV;
  }

  return panel->ops->disable(panel);
}

int pbl_panel_unprepare(const struct pbl_panel *panel) {
  if (!pbl_device_is_ready(&panel->dev)) {
    return -ENODEV;
  }

  return panel->ops->unprepare(panel);
}
