/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_light.h"

#include "syscall/syscall.h"

bool app_light_is_on(void) {
  return sys_light_is_on();
}

void app_light_enable_interaction(void) {
  sys_light_enable_interaction();
}

void app_light_enable(bool enable) {
  sys_light_enable(enable);
}

//! Expand a GColor8 argb byte (2 bits per channel) to a packed 0x00RRGGBB
//! value by bit-replicating each 2-bit component into 8 bits.
static uint32_t prv_argb_to_rgb888(uint8_t argb) {
  const uint8_t r2 = (argb >> 4) & 0x3;
  const uint8_t g2 = (argb >> 2) & 0x3;
  const uint8_t b2 = argb & 0x3;
  const uint8_t r = (uint8_t)((r2 << 6) | (r2 << 4) | (r2 << 2) | r2);
  const uint8_t g = (uint8_t)((g2 << 6) | (g2 << 4) | (g2 << 2) | g2);
  const uint8_t b = (uint8_t)((b2 << 6) | (b2 << 4) | (b2 << 2) | b2);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void app_light_set_color(GColor color) {
  sys_light_set_color_rgb888(prv_argb_to_rgb888(color.argb));
}

void app_light_set_color_rgb888(uint32_t rgb) {
  sys_light_set_color_rgb888(rgb);
}

void app_light_set_system_color(void) {
  sys_light_set_system_color();
}
