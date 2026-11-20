/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/panel.h>
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(driver_panel, CONFIG_DRIVER_PANEL_LOG_LEVEL);

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

int pbl_panel_set_h_flip(const struct pbl_panel *panel, bool flip) {
  if (!pbl_device_is_ready(&panel->dev)) {
    return -ENODEV;
  }

  if (panel->ops->set_h_flip == nullptr) {
    return -ENOTSUP;
  }

  return panel->ops->set_h_flip(panel, flip);
}
