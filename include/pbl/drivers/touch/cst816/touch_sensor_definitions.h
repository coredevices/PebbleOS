/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>

#include <board/board.h>

/**
 * @defgroup drivers_touch_cst816 CST816
 * @ingroup drivers_touch
 * @brief Hynitron CST816 touch controller board configuration.
 * @{
 */

/** @brief CST816 board configuration. */
typedef struct {
  /** I2C device in normal operation. */
  const struct pbl_i2c_dev *i2c;
  /** I2C device in boot (firmware update) mode. */
  const struct pbl_i2c_dev *i2c_boot;
  /** Interrupt line. */
  ExtiConfig int_exti;
  /** Reset line. */
  struct pbl_gpio reset;
  /** Maximum X coordinate. */
  uint16_t max_x;
  /** Maximum Y coordinate. */
  uint16_t max_y;
  /** Mirror the X axis. */
  bool invert_x_axis;
  /** Mirror the Y axis. */
  bool invert_y_axis;
} TouchSensor;

/** @} */
