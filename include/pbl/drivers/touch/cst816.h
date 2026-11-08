/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/touch.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_touch_cst816 CST816
 * @ingroup drivers_touch
 * @brief Hynitron CST816 touch controller.
 *
 * The chip pulses its interrupt line low when it has a sample or a gesture. It sleeps when the
 * touchscreen is disabled, and is reflashed at init when its firmware is not the bundled one.
 *
 * @code{.c}
 * PBL_CST816_DEFINE(s_cst816, &s_i2c3.bus, 0x15, 0x6A,
 *                   PBL_GPIO(SF32LB_GPIO1, 27, PBL_GPIO_PULL_UP),
 *                   PBL_GPIO(&s_npm1300_gpio.port, 2, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP),
 *                   .size_x = 200, .size_y = 228);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_cst816_state {
  const struct pbl_cst816 *cst;
  struct pbl_work work;
  struct pbl_irq_spec irq;
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
  /** Interrupt line, pulsed low by the chip. */
  struct pbl_gpio int_gpio;
  /** Reset line, active while the chip is held in reset. */
  struct pbl_gpio reset;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch_ops pbl_cst816_ops;
/** @endcond */

/**
 * @brief Define a CST816.
 *
 * @param sym Symbol of the instance; the touchscreen is @c sym.touch.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param _boot_addr 7-bit I2C address of the bootloader.
 * @param _int_gpio Interrupt line (struct pbl_gpio).
 * @param _reset Reset line (struct pbl_gpio).
 * @param ... Touchscreen properties, designated initializers of struct pbl_touch_props.
 */
#define PBL_CST816_DEFINE(sym, _bus, _addr, _boot_addr, _int_gpio, _reset, ...)         \
  PBL_TOUCH_STATE_DEFINE(sym);                                                          \
  static struct pbl_cst816_state sym##_cst816_state;                                    \
  const struct pbl_cst816 sym = {                                                       \
    .touch = PBL_TOUCH_INIT(sym, "cst816", &(_bus)->dev, &pbl_cst816_ops, __VA_ARGS__), \
    .state = &sym##_cst816_state,                                                       \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                    \
    .i2c_boot = PBL_I2C_DEV(_bus, _boot_addr),                                          \
    .int_gpio = _int_gpio,                                                              \
    .reset = _reset,                                                                    \
  };                                                                                    \
  PBL_DEVICE_REGISTER(sym, &sym.touch.dev)

/** @} */
