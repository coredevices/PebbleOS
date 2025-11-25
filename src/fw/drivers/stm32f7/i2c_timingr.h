/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// This is always invalid because it inclues a value being set in the reserved field.
#define I2C_TIMINGR_INVALID_VALUE (0xffffffff)

typedef enum I2CBusMode {
  I2CBusMode_Standard,  ///< I2C Standard Mode (up to 100 kHz)
  I2CBusMode_FastMode,  ///< I2C Fast Mode (up to 400 kHz)
  I2CBusMode_FastModePlus,  ///< I2C Fast Mode Plus (up to 1 MHz)
} I2CBusMode;

uint32_t i2c_timingr_calculate(uint32_t i2c_clk_frequency,
                               I2CBusMode bus_mode,
                               uint32_t target_bus_frequency,
                               uint32_t rise_time_ns,
                               uint32_t fall_time_ns);
