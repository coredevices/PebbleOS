/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/touch.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_touch_cst816 CST816
 * @ingroup drivers_touch
 * @brief Hynitron CST816 touch controller.
 *
 * The chip pulses its interrupt line low when it has a sample or a gesture. It sleeps when the
 * touchscreen is disabled, and is reflashed at init when its firmware is not the bundled one.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_cst816_state {
  const struct pbl_cst816 *cst;
  struct pbl_work work;
  uint64_t last_irq_ticks;
  bool enabled;
  bool palm_down;
  bool palm_click_pending;
};
/** @endcond */

/** @brief CST816. */
struct pbl_cst816 {
  /** Touchscreen, child of the I2C bus. */
  struct pbl_touch touch;
  /** Driver state. */
  struct pbl_cst816_state *state;
  /** I2C device in normal operation. */
  struct pbl_i2c_dev i2c;
  /** I2C device of the bootloader, used to update the firmware. */
  struct pbl_i2c_dev i2c_boot;
  /** Interrupt, pulsed low by the chip. */
  struct pbl_irq_spec irq;
  /** Reset line, active while the chip is held in reset. */
  struct pbl_gpio reset;
  /** Pin states, the default one for the interrupt line. */
  struct pbl_pinctrl pinctrl;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch_ops pbl_cst816_ops;

/** I2C address of the bootloader. */
#define PBL_CST816_BOOT_ADDR 0x6AU
/** @endcond */

/**
 * @brief Define a CST816.
 *
 * @param sym Symbol of the touchscreen.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _irq Interrupt, from PBL_IRQ_SPEC().
 * @param _reset Reset line, from PBL_GPIO().
 * @param _size_x Number of X coordinates it reports.
 * @param _size_y Number of Y coordinates it reports.
 * @param _pinctrl Default pin state, for the interrupt line, or NULL.
 */
#define PBL_CST816_DEFINE(sym, _bus, _addr, _irq, _reset, _size_x, _size_y, _pinctrl)          \
  PBL_TOUCH_STATE_DEFINE(sym);                                                                 \
  static struct pbl_cst816_state sym##_cst816_state;                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_cst816, struct pbl_touch, touch) {                       \
    .touch = PBL_TOUCH_INIT(sym, "cst816", &(_bus)->dev, &pbl_cst816_ops, .size_x = (_size_x), \
                            .size_y = (_size_y)),                                              \
    .state = &sym##_cst816_state, .i2c = PBL_I2C_DEV(_bus, _addr),                             \
    .i2c_boot = PBL_I2C_DEV(_bus, PBL_CST816_BOOT_ADDR), .irq = _irq, .reset = _reset,         \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                              \
  }

/** @} */
