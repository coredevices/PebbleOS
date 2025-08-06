/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#if MICRO_FAMILY_NRF5 || MICRO_FAMILY_SF32LB52
void i2c_hal_pins_set_gpio(I2CBus *bus);
void i2c_hal_pins_set_i2c(I2CBus *bus);
#endif
