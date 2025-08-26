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

#include <stdbool.h>
#include <stdint.h>


//! @file dbgserial_input.h
//! Contains the input related functionality of the debug serial port.

//! Initializes the input portions of the dbgserial driver.
void dbgserial_input_init(void);

typedef void (*DbgSerialCharacterCallback)(char c, bool* should_context_switch);
void dbgserial_register_character_callback(DbgSerialCharacterCallback callback);

void dbgserial_enable_rx_exti(void);

//! Enables/disables DMA-based receiving
void dbgserial_set_rx_dma_enabled(bool enabled);
