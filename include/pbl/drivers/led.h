/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_led LED
 * @ingroup drivers
 * @brief LEDs, single color or multicolor, driven by an LED controller.
 *
 * A multicolor LED mixes its channels after a color; a single color LED ignores it. Consumers,
 * such as a backlight on an LED, keep the state and set it whole.
 * @{
 */

/** Full brightness. */
#define PBL_LED_BRIGHTNESS_MAX 255U

struct pbl_led;

/** @brief LED driver operations. */
struct pbl_led_ops {
  /** Apply a brightness, 0 being off, and a color as 0xRRGGBB. */
  int (*set)(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color);
  /** Optional. Configure the controller again, then apply the brightness and color. */
  int (*refresh)(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color);
};

/** @brief LED. */
struct pbl_led {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_led_ops *ops;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_led_class;
/** @endcond */

/**
 * @brief Initialize a struct pbl_led.
 *
 * @param sym Symbol of the instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, e.g. the bus of the controller, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_LED_INIT(sym, _name, _init, _parent, _ops)                                 \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, nullptr, &pbl_led_class), \
    .ops = (_ops),                                                                     \
  }

/**
 * @brief Set an LED.
 *
 * @param led LED.
 * @param brightness Brightness, 0 (off) to @ref PBL_LED_BRIGHTNESS_MAX.
 * @param rgb_color Color as 0xRRGGBB, for multicolor LEDs.
 * @return 0, @c -ENODEV if the LED is not ready, or another negative errno.
 */
int pbl_led_set(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color);

/**
 * @brief Configure the controller of an LED again and set the LED, after the controller may have
 *        lost its configuration.
 *
 * @param led LED.
 * @param brightness Brightness, 0 (off) to @ref PBL_LED_BRIGHTNESS_MAX.
 * @param rgb_color Color as 0xRRGGBB, for multicolor LEDs.
 * @return 0, @c -ENODEV if the LED is not ready, or another negative errno.
 */
int pbl_led_refresh(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color);

/** @} */
