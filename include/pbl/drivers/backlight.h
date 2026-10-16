/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_backlight Backlight
 * @ingroup drivers
 * @brief Backlight class.
 *
 * A backlight is a struct pbl_backlight device. The class keeps the brightness and color last
 * applied, which drivers read from the state. Every call into the driver runs with the device lock
 * held, a recursive mutex.
 *
 * @code{.c}
 * pbl_backlight_set_brightness(BACKLIGHT, 60);
 * ...
 * pbl_backlight_set_brightness(BACKLIGHT, 0);
 * @endcode
 * @{
 */

/**
 * @name Backlight colors
 * Colors for pbl_backlight_set_color(), as 0xRRGGBB. Not gamma-corrected.
 * @{
 */
/** @brief Red. */
#define BACKLIGHT_COLOR_RED 0xFF0000
/** @brief Green. */
#define BACKLIGHT_COLOR_GREEN 0x00FF00
/** @brief Blue. */
#define BACKLIGHT_COLOR_BLUE 0x0000FF
/** @brief Black (off). */
#define BACKLIGHT_COLOR_BLACK 0x000000
/** @brief White. */
#define BACKLIGHT_COLOR_WHITE 0xFFFFFF
/** @brief Warm white. */
#define BACKLIGHT_COLOR_WARM_WHITE 0xFFBFA2
/** @} */

/** @brief Maximum brightness. */
#define PBL_BACKLIGHT_BRIGHTNESS_MAX 100U

struct pbl_backlight;

/** @brief Backlight driver operations, called with the device lock held. */
struct pbl_backlight_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_backlight *bl);
  /** Apply @p brightness; the state still holds the previous one. */
  int (*set_brightness)(const struct pbl_backlight *bl, uint8_t brightness);
  /**
   * Optional. Map a brightness to a hardware level identifier, for drivers with fewer levels
   * than brightness steps.
   */
  uint8_t (*get_level)(const struct pbl_backlight *bl, uint8_t brightness);
  /** Optional. Re-apply the state to the hardware. */
  int (*refresh)(const struct pbl_backlight *bl);
  /** Optional. Apply @p rgb_color; the state still holds the previous one. */
  int (*set_color)(const struct pbl_backlight *bl, uint32_t rgb_color);
};

/** @brief Backlight runtime state. */
struct pbl_backlight_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Brightness, 0 (off) to @ref PBL_BACKLIGHT_BRIGHTNESS_MAX. */
  uint8_t brightness;
  /** Color as 0xRRGGBB, on backlights with color. */
  uint32_t color;
};

/** @brief Backlight. */
struct pbl_backlight {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_backlight_ops *ops;
  /** Runtime state. */
  struct pbl_backlight_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_backlight_class;
int pbl_backlight_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's backlight. */
extern const struct pbl_backlight *const BACKLIGHT;

/**
 * @brief Define the class state of a backlight.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_BACKLIGHT_STATE_DEFINE(sym)                       \
  PBL_DEVICE_STATE_DEFINE(sym);                               \
  static struct pbl_backlight_state sym##_backlight_state = { \
    .lock = PBL_MUTEX_INITIALIZER,                            \
    .color = BACKLIGHT_COLOR_WARM_WHITE,                      \
  }

/**
 * @brief Initialize a struct pbl_backlight.
 *
 * @param sym Symbol of the instance, as passed to PBL_BACKLIGHT_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 */
#define PBL_BACKLIGHT_INIT(sym, _name, _parent, _ops)                                  \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_backlight_dev_init, _parent, nullptr, \
                                 &pbl_backlight_class),                                \
    .ops = (_ops),                                                                     \
    .state = &sym##_backlight_state,                                                   \
  }

/**
 * @brief Set the brightness.
 *
 * @param bl Backlight.
 * @param brightness Brightness from 0 (off) to @ref PBL_BACKLIGHT_BRIGHTNESS_MAX; larger values
 *                   are clamped.
 * @return 0, -ENODEV if the backlight is not ready, or a negative errno.
 */
int pbl_backlight_set_brightness(const struct pbl_backlight *bl, uint8_t brightness);

/**
 * @brief Map a brightness to an opaque hardware level identifier.
 *
 * Two brightness values with the same identifier produce identical output, which lets callers
 * collapse steps the hardware cannot distinguish. Backlights with continuous control return the
 * brightness unchanged.
 *
 * @param bl Backlight.
 * @param brightness Brightness from 0 to @ref PBL_BACKLIGHT_BRIGHTNESS_MAX.
 * @return Hardware level identifier.
 */
uint8_t pbl_backlight_get_level(const struct pbl_backlight *bl, uint8_t brightness);

/**
 * @brief Re-apply the brightness and color to the hardware.
 *
 * A no-op while the backlight is off, or on drivers that cannot re-apply without a visible
 * glitch.
 *
 * @param bl Backlight.
 * @return 0, -ENODEV if the backlight is not ready, or a negative errno.
 */
int pbl_backlight_refresh(const struct pbl_backlight *bl);

/**
 * @brief Set the color.
 *
 * @param bl Backlight.
 * @param rgb_color Color as 0xRRGGBB, e.g. one of the @c BACKLIGHT_COLOR_* values.
 * @return 0, -ENOTSUP if the backlight has no color, -ENODEV if it is not ready, or a negative
 *         errno.
 */
int pbl_backlight_set_color(const struct pbl_backlight *bl, uint32_t rgb_color);

/**
 * @brief Get the color.
 *
 * @param bl Backlight.
 * @return Color as 0xRRGGBB.
 */
uint32_t pbl_backlight_get_color(const struct pbl_backlight *bl);

/** @} */
