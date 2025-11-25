/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Call this as you're just entering manufacturing mode to do initalial setup.
void accessory_mfg_mode_start(void);

//! Called on an ISR to handle a character from the accessory connector.
bool accessory_mfg_mode_handle_char(char c);

