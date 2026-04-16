/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/backlight.h"

#include "board/board.h"
#include "console/prompt.h"

#include <stdlib.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// Display register offsets (must match QEMU pebble-display)
#define DISP_CTRL        0x000
#define DISP_BRIGHTNESS  0x018
#define CTRL_UPDATE      (1 << 1)

// Brightness levels for QEMU display
#define BACKLIGHT_OFF_LEVEL  180
#define BACKLIGHT_ON_LEVEL   255

static bool s_initialized;

void backlight_init(void) {
  // Start with dim (backlight off) state
  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = BACKLIGHT_OFF_LEVEL;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
  s_initialized = true;
}

void backlight_set_brightness(uint16_t brightness) {
  if (!s_initialized) {
    return;
  }

  // Map PebbleOS brightness (0..BACKLIGHT_BRIGHTNESS_MAX=65535) to display level
  uint32_t level;
  if (brightness == 0) {
    level = BACKLIGHT_OFF_LEVEL;
  } else {
    uint32_t range = BACKLIGHT_ON_LEVEL - BACKLIGHT_OFF_LEVEL;
    level = BACKLIGHT_OFF_LEVEL + ((uint32_t)brightness * range) / BACKLIGHT_BRIGHTNESS_MAX;
    if (level > BACKLIGHT_ON_LEVEL) {
      level = BACKLIGHT_ON_LEVEL;
    }
  }

  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = level;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
}

void command_backlight_ctl(const char *arg) {
  const int bright_percent = atoi(arg);
  if (bright_percent < 0 || bright_percent > 100) {
    prompt_send_response("Invalid Brightness");
    return;
  }
  backlight_set_brightness((BACKLIGHT_BRIGHTNESS_MAX * bright_percent) / 100);
  prompt_send_response("OK");
}
