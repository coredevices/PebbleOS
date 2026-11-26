/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display_bridge.h>

PBL_DEVICE_CLASS_DEFINE(pbl_display_bridge_class, "display_bridge");

int pbl_display_bridge_refresh(const struct pbl_display_bridge *bridge) {
  return bridge->ops->refresh(bridge);
}

void pbl_display_bridge_hold(const struct pbl_display_bridge *bridge, bool hold) {
  bridge->ops->hold(bridge, hold);
}

int pbl_display_bridge_request_tear(const struct pbl_display_bridge *bridge) {
  return bridge->ops->request_tear(bridge);
}

int pbl_display_bridge_set_event_cb(const struct pbl_display_bridge *bridge,
                                    pbl_display_bridge_event_cb_t cb, void *arg) {
  if (!pbl_device_is_ready(&bridge->dev)) {
    return -ENODEV;
  }

  bridge->ops->set_event_cb(bridge, cb, arg);

  return 0;
}
