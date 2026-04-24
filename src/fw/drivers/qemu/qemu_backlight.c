/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/backlight.h"

#include "board/board.h"
#include "console/prompt.h"
#if CAPABILITY_HAS_COLOR_BACKLIGHT
#include "drivers/led_controller.h"
#endif

#include <stdlib.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// Display register offsets (must match QEMU pebble-display)
#define DISP_CTRL        0x000
#define DISP_BRIGHTNESS  0x018
#define CTRL_UPDATE      (1 << 1)

// Brightness levels for QEMU display grayscale path
#define BACKLIGHT_OFF_LEVEL  180
#define BACKLIGHT_ON_LEVEL   255

static bool s_initialized;

void backlight_init(void) {
#if CAPABILITY_HAS_COLOR_BACKLIGHT
  // RGB LED is the sole light source — grayscale BRIGHTNESS reg stays at
  // 0xFF identity so only the RGB channels modulate light output.
  led_controller_init();
#else
  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = BACKLIGHT_OFF_LEVEL;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
#endif
  s_initialized = true;
}

void backlight_set_brightness(uint8_t brightness) {
  if (!s_initialized) {
    return;
  }

#if CAPABILITY_HAS_COLOR_BACKLIGHT
  led_controller_backlight_set_brightness(brightness);
#else
  uint32_t level;
  if (brightness == 0) {
    level = BACKLIGHT_OFF_LEVEL;
  } else {
    uint32_t range = BACKLIGHT_ON_LEVEL - BACKLIGHT_OFF_LEVEL;
    level = BACKLIGHT_OFF_LEVEL + ((uint32_t)brightness * range) / 100;
    if (level > BACKLIGHT_ON_LEVEL) {
      level = BACKLIGHT_ON_LEVEL;
    }
  }

  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = level;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
#endif
}

void command_backlight_ctl(const char *arg) {
  const int bright_percent = atoi(arg);
  if (bright_percent < 0 || bright_percent > 100) {
    prompt_send_response("Invalid Brightness");
    return;
  }
  backlight_set_brightness(bright_percent);
  prompt_send_response("OK");
}
