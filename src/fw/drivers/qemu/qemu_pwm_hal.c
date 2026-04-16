/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pwm.h"

#include "board/board.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

// Display brightness register offset (must match QEMU pebble-display)
#define DISP_BRIGHTNESS  0x018
#define DISP_CTRL        0x000
#define CTRL_UPDATE      (1 << 1)

// Minimum brightness when backlight is "off" - simulates ambient visibility
#define BACKLIGHT_OFF_BRIGHTNESS  40

static uint32_t s_resolution = 1024;

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency) {
  (void)pwm;
  (void)frequency;
  if (resolution > 0) {
    s_resolution = resolution;
  }
}

void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle) {
  (void)pwm;
  // Map duty_cycle (0..s_resolution) to brightness range
  // 0 duty = dim (BACKLIGHT_OFF_BRIGHTNESS), full duty = full bright (255)
  uint32_t range = 255 - BACKLIGHT_OFF_BRIGHTNESS;
  uint32_t brightness = BACKLIGHT_OFF_BRIGHTNESS + (duty_cycle * range) / s_resolution;
  if (brightness > 255) {
    brightness = 255;
  }
  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = brightness;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
}

void pwm_enable(const PwmConfig *pwm, bool enable) {
  (void)pwm;
  uint32_t brightness = enable ? 255 : BACKLIGHT_OFF_BRIGHTNESS;
  REG32(QEMU_DISPLAY_BASE + DISP_BRIGHTNESS) = brightness;
  REG32(QEMU_DISPLAY_BASE + DISP_CTRL) |= CTRL_UPDATE;
}
