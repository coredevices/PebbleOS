/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>

typedef const struct TouchSensor {
  I2CSlavePort *i2c;
  InputConfig int_gpio;
  ExtiConfig int_exti;
  OutputConfig reset_gpio;
} TouchSensor;
