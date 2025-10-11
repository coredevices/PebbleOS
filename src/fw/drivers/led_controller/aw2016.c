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

#include "drivers/led_controller.h"

#include "board/board.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "system/logging.h"
#include "system/passert.h"

static bool s_backlight_off;
static bool s_initialized = false;
static uint32_t s_rgb_current_color = LED_BLACK;

void led_controller_init(void) {


}

void led_controller_backlight_set_brightness(uint8_t brightness) {

}


void led_controller_rgb_set_color(uint32_t rgb_color) {

}

uint32_t led_controller_rgb_get_color(void) {
  return s_rgb_current_color;
}

void command_rgb_set_color(const char* color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}

