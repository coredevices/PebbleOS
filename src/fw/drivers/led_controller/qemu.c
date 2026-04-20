/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/led_controller.h"

#include "board/board.h"

#include <stdlib.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// Display register offsets (must match QEMU pebble-display)
#define DISP_BL_RED   0x024
#define DISP_BL_GREEN 0x028
#define DISP_BL_BLUE  0x02C

static uint8_t s_brightness = 100U;
static uint32_t s_rgb_current_color = LED_WHITE;

static void prv_write_channels(uint8_t r, uint8_t g, uint8_t b) {
  REG32(QEMU_DISPLAY_BASE + DISP_BL_RED) = r;
  REG32(QEMU_DISPLAY_BASE + DISP_BL_GREEN) = g;
  REG32(QEMU_DISPLAY_BASE + DISP_BL_BLUE) = b;
}

void led_controller_init(void) {
  prv_write_channels(0xFFU, 0xFFU, 0xFFU);
}

void led_controller_backlight_set_brightness(uint8_t brightness) {
  if (brightness > 100U) {
    brightness = 100U;
  }
  s_brightness = brightness;
  led_controller_rgb_set_color(s_rgb_current_color);
}

void led_controller_rgb_set_color(uint32_t rgb_color) {
  uint8_t r = ((rgb_color >> 16) & 0xFFU) * s_brightness / 100U;
  uint8_t g = ((rgb_color >> 8) & 0xFFU) * s_brightness / 100U;
  uint8_t b = (rgb_color & 0xFFU) * s_brightness / 100U;

  prv_write_channels(r, g, b);
  s_rgb_current_color = rgb_color;
}

uint32_t led_controller_rgb_get_color(void) {
  return s_rgb_current_color;
}

void command_rgb_set_color(const char *color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}
