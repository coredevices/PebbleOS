/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "board/board.h"

#define RESET_PIN_CTRLBY_NPM1300          1

typedef struct {
  I2CSlavePort *i2c;
  I2CSlavePort *i2c_boot;
  ExtiConfig int_exti;
} TouchSensor;
