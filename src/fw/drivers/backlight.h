/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// The light brightness can vary between LIGHT_BRIGHTNESS_OFF and LIGHT_BRIGHTNESS_ON
#define BACKLIGHT_BRIGHTNESS_OFF  0x0000
#define BACKLIGHT_BRIGHTNESS_MAX  0x4000

void backlight_init(void);

//! @param brightness a number between BACKLIGHT_BRIGHTNESS_OFF and BACKLIGHT_BRIGHTNESS_ON
void backlight_set_brightness(uint16_t brightness);