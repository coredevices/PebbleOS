/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/touch.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_touch_ft6236 FT6x36
 * @ingroup drivers_touch
 * @brief @ref drivers_touch driver for the FocalTech FT6x36 family, such as the FT6236 and the
 * FT6146.
 *
 * Reports the first contact. The chip pulses its interrupt line low for each sample while
 * touched, and once more on release. It sleeps in hibernation while the touchscreen is off, out of
 * which only a reset wakes it.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_ft6236;

struct pbl_ft6236_state {
  const struct pbl_ft6236 *ft;
  struct pbl_work work;
  bool enabled;
};
/** @endcond */

/** @brief FT6x36. */
struct pbl_ft6236 {
  /** Touchscreen, child of the I2C bus. */
  struct pbl_touch touch;
  /** Driver state. */
  struct pbl_ft6236_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Interrupt, pulsed low by the chip. */
  struct pbl_irq_spec irq;
  /** Reset line, active while the chip is held in reset. */
  struct pbl_gpio reset;
  /** Pin states, the default one for the interrupt line. */
  struct pbl_pinctrl pinctrl;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch_ops pbl_ft6236_ops;
/** @endcond */

/**
 * @brief Define an FT6x36.
 *
 * @param sym Symbol of the touchscreen.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _irq Interrupt, from PBL_IRQ_SPEC().
 * @param _reset Reset line, from PBL_GPIO().
 * @param _size_x Number of X coordinates it reports.
 * @param _size_y Number of Y coordinates it reports.
 * @param _inverted_x X axis is inverted.
 * @param _inverted_y Y axis is inverted.
 * @param _swapped_x_y X and Y axes are swapped.
 * @param _pinctrl Default pin state, for the interrupt line, or NULL.
 */
#define PBL_FT6236_DEFINE(sym, _bus, _addr, _irq, _reset, _size_x, _size_y, _inverted_x,         \
                          _inverted_y, _swapped_x_y, _pinctrl)                                   \
  PBL_TOUCH_STATE_DEFINE(sym);                                                                   \
  static struct pbl_ft6236_state sym##_ft6236_state;                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ft6236, struct pbl_touch, touch) {                         \
    .touch = PBL_TOUCH_INIT(sym, "ft6236", &(_bus)->dev, &pbl_ft6236_ops, .size_x = (_size_x),   \
                            .size_y = (_size_y), .inverted_x = (_inverted_x),                    \
                            .inverted_y = (_inverted_y), .swapped_x_y = (_swapped_x_y)),         \
    .state = &sym##_ft6236_state, .i2c = PBL_I2C_DEV(_bus, _addr), .irq = _irq, .reset = _reset, \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                \
  }

/** @} */
