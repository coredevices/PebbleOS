/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_phy PHY
 * @ingroup drivers
 * @brief PHY class.
 *
 * A PHY is the transceiver of a controller, as the USB PHY of a USB controller. The controller
 * takes it from its devicetree `phys` property, powers it on before using the bus and off after.
 *
 * A USB PHY that senses VBUS reports it: pbl_phy_vbus_present() tells whether it is there, and
 * the listeners added with pbl_phy_add_listener() hear it come and go, from thread context.
 *
 * @code{.c}
 * ret = pbl_phy_power_on(phy);
 * ...
 * ret = pbl_phy_power_off(phy);
 * @endcode
 * @{
 */

struct pbl_phy;

/** @brief PHY listener. */
struct pbl_phy_listener {
  /** VBUS came or went; pbl_phy_vbus_present() tells which. */
  void (*vbus_changed)(const struct pbl_phy *phy, struct pbl_phy_listener *listener);
  /** @cond INTERNAL_HIDDEN */
  struct pbl_phy_listener *next;
  /** @endcond */
};

/** @brief PHY driver operations, called with the device lock held. */
struct pbl_phy_ops {
  /** Optional. Set the PHY up, powered off. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_phy *phy);
  /** Power the PHY on. Returns 0 or a negative errno. */
  int (*power_on)(const struct pbl_phy *phy);
  /** Power the PHY off. Returns 0 or a negative errno. */
  int (*power_off)(const struct pbl_phy *phy);
  /** Optional, for a PHY that senses VBUS. Returns 0 or a negative errno. */
  int (*vbus_present)(const struct pbl_phy *phy, bool *present);
};

/** @brief PHY runtime state. */
struct pbl_phy_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Listeners. */
  struct pbl_phy_listener *listeners;
};

/** @brief PHY. */
struct pbl_phy {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_phy_ops *ops;
  /** Runtime state. */
  struct pbl_phy_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_phy_class;

int pbl_phy_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the runtime state of a PHY.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_PHY_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_phy_state sym##_phy_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_phy.
 *
 * @param sym Symbol of the instance, as passed to PBL_PHY_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_PHY_INIT(sym, _name, _parent, _ops)                                          \
  {                                                                                      \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_phy_dev_init, _parent, &pbl_phy_class), \
    .ops = (_ops),                                                                       \
    .state = &sym##_phy_state,                                                           \
  }

/**
 * @brief Power a PHY on.
 *
 * @param phy PHY.
 * @return 0 or a negative errno.
 */
int pbl_phy_power_on(const struct pbl_phy *phy);

/**
 * @brief Power a PHY off.
 *
 * @param phy PHY.
 * @return 0 or a negative errno.
 */
int pbl_phy_power_off(const struct pbl_phy *phy);

/**
 * @brief Check whether VBUS is present.
 *
 * @param phy PHY.
 * @param[out] present True while VBUS is present.
 * @return 0, @c -ENOTSUP if the PHY does not sense VBUS, or another negative errno.
 */
int pbl_phy_vbus_present(const struct pbl_phy *phy, bool *present);

/**
 * @brief Add a listener to a PHY.
 *
 * @param phy PHY.
 * @param listener Listener, not added yet, which stays in use.
 */
void pbl_phy_add_listener(const struct pbl_phy *phy, struct pbl_phy_listener *listener);

/**
 * @brief Report VBUS coming or going to the listeners, from thread context.
 *
 * For drivers.
 *
 * @param phy PHY.
 */
void pbl_phy_report_vbus_changed(const struct pbl_phy *phy);

/** @} */
