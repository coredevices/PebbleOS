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

#include <stdint.h>

//! FIXME: These colors are not gamma-corrected, so will not match normal
//! RGB color values, 100 is max brightness
#define LED_BLACK       0x00000000
#define LED_RED         0x00640000
#define LED_GREEN       0x00006400
#define LED_BLUE        0x00000064
#define LED_ORANGE      0x00285F00
#define LED_WHITE       0x00FFFFFF

#define LED_DIM_GREEN   0x00003C00  // Low power version for charging indicator
#define LED_DIM_ORANGE  0x000F2300  // Low power version for charging indicator

void led_controller_init(void);

// Not sure these are the correct functions to define atm, but it is fine as a first pass
void led_controller_backlight_set_brightness(uint8_t brightness);

void led_controller_rgb_set_color(uint32_t rgb_color);

uint32_t led_controller_rgb_get_color(void);

