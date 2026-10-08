/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <board/board.h>

/**
 * @defgroup drivers_touch_cst816 CST816
 * @ingroup drivers_touch
 * @brief Hynitron CST816 touch controller board configuration.
 * @{
 */

#ifdef CONFIG_BOARD_OBELIX
/** @brief The reset line is driven by nPM1300 GPIO2 instead of @ref TouchSensor::reset. */
#define RESET_PIN_CTRLBY_NPM1300 1
#endif

/** @brief CST816 board configuration. */
typedef struct {
  /** I2C device in normal operation. */
  I2CSlavePort *i2c;
  /** I2C device in boot (firmware update) mode. */
  I2CSlavePort *i2c_boot;
  /** Interrupt line. */
  ExtiConfig int_exti;
  /** Reset line. */
  OutputConfig reset;
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
