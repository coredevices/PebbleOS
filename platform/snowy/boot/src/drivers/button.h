/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "button_id.h"

#include <stdbool.h>
#include <stdint.h>

void button_init(void);

bool button_is_pressed(ButtonId id);
uint8_t button_get_state_bits(void);

void button_interrupt_handler(void* button_id);

bool button_selftest(void);
