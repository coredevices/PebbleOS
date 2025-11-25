/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! FIXME: These colors are not gamma-corrected, so will not match normal
//! RGB color values, 100 is max brightness
#define LED_BLACK       0x00000000
#define LED_RED         0x00640000
#define LED_GREEN       0x00006400
#define LED_BLUE        0x00000064
#define LED_ORANGE      0x00285F00
#define LED_WHITE       0x00FFFFFF
#define LED_WARM_WHITE  0x00F0D0B0

#define LED_DIM_GREEN   0x00003C00  // Low power version for charging indicator
#define LED_DIM_ORANGE  0x000F2300  // Low power version for charging indicator

void led_controller_init(void);

// Not sure these are the correct functions to define atm, but it is fine as a first pass
void led_controller_backlight_set_brightness(uint8_t brightness);

void led_controller_rgb_set_color(uint32_t rgb_color);

uint32_t led_controller_rgb_get_color(void);

