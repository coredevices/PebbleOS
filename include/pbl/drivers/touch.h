/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_touch Touchscreen
 * @ingroup drivers
 * @brief Touchscreen class.
 *
 * A touchscreen is a struct pbl_touch device and an @ref input device: its driver reports
 * samples (@ref PBL_INPUT_BTN_TOUCH, @ref PBL_INPUT_ABS_X and @ref PBL_INPUT_ABS_Y) and the
 * gestures the controller recognizes through pbl_touch_report() and pbl_touch_report_gesture(),
 * and input listeners consume them.
 *
 * The board describes the touchscreen with the devicetree touchscreen properties
 * (struct pbl_touch_props): the class maps the controller coordinates to the display with them
 * before reporting.
 *
 * The touchscreen is powered while it has users, counted by pbl_touch_enable() and
 * pbl_touch_disable(). Every call into the driver runs with the device lock held, a recursive
 * mutex. Drivers report without it, so that listeners may enable or disable the touchscreen.
 *
 * @code{.c}
 * pbl_touch_enable(TOUCH);
 * // ...
 * pbl_touch_disable(TOUCH);
 * @endcode
 * @{
 */

struct pbl_touch;

/** @brief Touchscreen properties, the devicetree @c touchscreen-* properties. */
struct pbl_touch_props {
  /** Number of X coordinates the controller reports, @c touchscreen-size-x. */
  uint16_t size_x;
  /** Number of Y coordinates the controller reports, @c touchscreen-size-y. */
  uint16_t size_y;
  /** X axis is inverted, @c touchscreen-inverted-x. */
  bool inverted_x;
  /** Y axis is inverted, @c touchscreen-inverted-y. */
  bool inverted_y;
  /** X and Y axes are swapped, after inversion, @c touchscreen-swapped-x-y. */
  bool swapped_x_y;
};

/** @brief Touchscreen driver operations, called with the device lock held. */
struct pbl_touch_ops {
  /**
   * Bring the hardware up, leaving it powered down. Returns 0, -ENODEV if the part does not
   * answer, or a negative errno.
   */
  int (*init)(const struct pbl_touch *touch);
  /** Power the touchscreen up and start reporting. */
  int (*enable)(const struct pbl_touch *touch);
  /** Stop reporting and power the touchscreen down. */
  int (*disable)(const struct pbl_touch *touch);
};

/** @brief Last sample reported by a touchscreen. */
struct pbl_touch_sample {
  /** Touch contact. */
  bool down;
  /** X coordinate. */
  int16_t x;
  /** Y coordinate. */
  int16_t y;
};

/** @brief Touchscreen runtime state. */
struct pbl_touch_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Users. */
  uint16_t users;
  /** Last sample reported. */
  struct pbl_touch_sample last;
};

/** @brief Touchscreen. */
struct pbl_touch {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_touch_ops *ops;
  /** Runtime state. */
  struct pbl_touch_state *state;
  /** Properties. */
  struct pbl_touch_props props;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_touch_class;
int pbl_touch_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's touchscreen. */
extern const struct pbl_touch *const TOUCH;

/**
 * @brief Define the class state of a touchscreen.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_TOUCH_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);     \
  static struct pbl_touch_state sym##_touch_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_touch.
 *
 * @param sym Symbol of the instance, as passed to PBL_TOUCH_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 * @param ... Properties, designated initializers of struct pbl_touch_props.
 */
#define PBL_TOUCH_INIT(sym, _name, _parent, _ops, ...)                                             \
  {                                                                                                \
    .dev =                                                                                         \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_touch_dev_init, _parent, nullptr, &pbl_touch_class), \
    .ops = (_ops),                                                                                 \
    .state = &sym##_touch_state,                                                                   \
    .props = {__VA_ARGS__},                                                                        \
  }

/**
 * @brief Take a reference on the touchscreen, powering it up on the first one.
 *
 * @param touch Touchscreen.
 * @return 0, -ENODEV if the touchscreen is not ready, or a negative errno, in which case no
 *         reference is taken.
 */
int pbl_touch_enable(const struct pbl_touch *touch);

/**
 * @brief Drop a reference taken with pbl_touch_enable(), powering the touchscreen down on the last
 * one.
 *
 * @param touch Touchscreen.
 * @return 0, -ENODEV if the touchscreen is not ready, -EALREADY if it has no users, or a negative
 *         errno, in which case the reference is dropped anyway.
 */
int pbl_touch_disable(const struct pbl_touch *touch);

/**
 * @brief Check whether the touchscreen has users.
 *
 * @param touch Touchscreen.
 * @return true while powered up.
 */
bool pbl_touch_is_enabled(const struct pbl_touch *touch);

/**
 * @brief Get the last sample reported.
 *
 * @param touch Touchscreen.
 * @param[out] sample Last sample, in display coordinates.
 */
void pbl_touch_get_last(const struct pbl_touch *touch, struct pbl_touch_sample *sample);

/**
 * @brief Take the device lock.
 *
 * Drivers take it around the hardware access of their interrupt bottom half.
 *
 * @param touch Touchscreen.
 */
void pbl_touch_lock(const struct pbl_touch *touch);

/**
 * @brief Release the device lock.
 *
 * @param touch Touchscreen.
 */
void pbl_touch_unlock(const struct pbl_touch *touch);

/**
 * @brief Report a sample.
 *
 * Called by the driver from thread context without the device lock held.
 *
 * @param touch Touchscreen.
 * @param down Whether there is a touch contact.
 * @param x X coordinate reported by the controller.
 * @param y Y coordinate reported by the controller.
 */
void pbl_touch_report(const struct pbl_touch *touch, bool down, uint16_t x, uint16_t y);

/**
 * @brief Report a gesture recognized by the controller.
 *
 * Called by the driver from thread context without the device lock held.
 *
 * @param touch Touchscreen.
 * @param code @ref PBL_INPUT_EV_GES code.
 * @param x X coordinate reported by the controller.
 * @param y Y coordinate reported by the controller.
 */
void pbl_touch_report_gesture(const struct pbl_touch *touch, uint16_t code, uint16_t x, uint16_t y);

/** @} */
