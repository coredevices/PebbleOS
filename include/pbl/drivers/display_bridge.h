/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_display_bridge Display bridge
 * @ingroup drivers
 * @brief Display bridges.
 *
 * A bridge is what a display controller sends its pixels through on their way to the panel, as a
 * MIPI DSI host, found at the output port of the controller. To a panel with its own frame
 * memory, the controller has the bridge send it a frame on each refresh, timed by the tearing
 * effect of the panel, which the bridge reports. The controller holds the bridge while it sets
 * itself up for the next refresh.
 *
 * @code{.c}
 * ret = pbl_display_bridge_set_event_cb(bridge, prv_event, ctx);
 * ret = pbl_display_bridge_request_tear(bridge);
 * // From prv_event() on PBL_DISPLAY_BRIDGE_EVENT_TEAR
 * ret = pbl_display_bridge_refresh(bridge);
 * @endcode
 * @{
 */

/** @brief Events of a bridge. */
enum pbl_display_bridge_event {
  /** The panel reported its tearing effect, once asked for with pbl_display_bridge_request_tear().
   */
  PBL_DISPLAY_BRIDGE_EVENT_TEAR,
  /** The frame of a refresh is out. */
  PBL_DISPLAY_BRIDGE_EVENT_REFRESH_DONE,
};

/**
 * @brief Called on an event of a bridge, from its interrupt.
 *
 * @param event Event.
 * @param arg Argument given with the callback.
 */
typedef void (*pbl_display_bridge_event_cb_t)(enum pbl_display_bridge_event event, void *arg);

struct pbl_display_bridge;

/** @brief Bridge operations, callable from an interrupt. */
struct pbl_display_bridge_ops {
  /** Send a frame from the display controller. Returns 0 or a negative errno. */
  int (*refresh)(const struct pbl_display_bridge *bridge);
  /** Stop taking from the display controller while it is set up, or go on. */
  void (*hold)(const struct pbl_display_bridge *bridge, bool hold);
  /** Report the next tearing effect of the panel. Returns 0 or a negative errno. */
  int (*request_tear)(const struct pbl_display_bridge *bridge);
  /** Set the callback of the events. */
  void (*set_event_cb)(const struct pbl_display_bridge *bridge, pbl_display_bridge_event_cb_t cb,
                       void *arg);
};

/** @brief A bridge. */
struct pbl_display_bridge {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_display_bridge_ops *ops;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_display_bridge_class;
/** @endcond */

/**
 * @brief Initializer for the struct pbl_display_bridge of bridge @p sym, with its device state
 * defined by PBL_DEVICE_STATE_DEFINE().
 *
 * @param sym Symbol of the bridge instance.
 * @param _name Name.
 * @param _parent Parent device, as the host the bridge is part of, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_DISPLAY_BRIDGE_INIT(sym, _name, _parent, _ops)                                 \
  {                                                                                        \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, nullptr, _parent, &pbl_display_bridge_class), \
    .ops = (_ops),                                                                         \
  }

/**
 * @brief Send a frame from the display controller to the panel.
 *
 * The end of the frame is reported with @ref PBL_DISPLAY_BRIDGE_EVENT_REFRESH_DONE. Callable from
 * an interrupt, as the event callback.
 *
 * @param bridge Bridge.
 * @return 0 or a negative errno.
 */
int pbl_display_bridge_refresh(const struct pbl_display_bridge *bridge);

/**
 * @brief Stop taking from the display controller while it is set up, or go on.
 *
 * Callable from an interrupt, as the event callback.
 *
 * @param bridge Bridge.
 * @param hold Stop, or go on.
 */
void pbl_display_bridge_hold(const struct pbl_display_bridge *bridge, bool hold);

/**
 * @brief Report the next tearing effect of the panel, with @ref PBL_DISPLAY_BRIDGE_EVENT_TEAR.
 *
 * @param bridge Bridge.
 * @return 0 or a negative errno.
 */
int pbl_display_bridge_request_tear(const struct pbl_display_bridge *bridge);

/**
 * @brief Set the callback of the events of a bridge.
 *
 * @param bridge Bridge.
 * @param cb Callback, or NULL.
 * @param arg Passed to @p cb.
 * @return 0, or @c -ENODEV if the bridge is not ready.
 */
int pbl_display_bridge_set_event_cb(const struct pbl_display_bridge *bridge,
                                    pbl_display_bridge_event_cb_t cb, void *arg);

/** @} */
