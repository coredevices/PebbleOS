/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>

void i2c_hal_init(I2CBus *bus);

void i2c_hal_enable(I2CBus *bus);

void i2c_hal_disable(I2CBus *bus);

bool i2c_hal_is_busy(I2CBus *bus);

void i2c_hal_abort_transfer(I2CBus *bus);

void i2c_hal_init_transfer(I2CBus *bus);

void i2c_hal_start_transfer(I2CBus *bus);
