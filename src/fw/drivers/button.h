/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "button_id.h"

#include <stdbool.h>
#include <stdint.h>

void button_init(void);

void button_set_rotated(bool rotated);

bool button_is_pressed(ButtonId id);
uint8_t button_get_state_bits(void);

bool button_selftest(void);
