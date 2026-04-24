/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void backlight_init(void);

//! @param brightness Brightness level (0-100)
void backlight_set_brightness(uint8_t brightness);