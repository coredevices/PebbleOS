/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! FIXME: These colors are not gamma-corrected
#define BACKLIGHT_COLOR_RED         0xFF0000
#define BACKLIGHT_COLOR_GREEN       0x00FF00
#define BACKLIGHT_COLOR_BLUE        0x0000FF
#define BACKLIGHT_COLOR_BLACK       0x000000
#define BACKLIGHT_COLOR_WHITE       0xFFFFFF
#define BACKLIGHT_COLOR_WARM_WHITE  0xFFBFA2

void backlight_init(void);

//! @param brightness Brightness level (0-100)
void backlight_set_brightness(uint8_t brightness);

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
void backlight_set_color(uint32_t rgb_color);

uint32_t backlight_get_color(void);
#endif