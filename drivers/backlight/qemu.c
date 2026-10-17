/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/qemu.h>
#include <pbl/kernel/mmio.h>

#include <board/board.h>

// Display register offsets (must match QEMU pebble-display)
#define DISP_CTRL       0x000
#define DISP_BRIGHTNESS 0x018
#define DISP_BL_RED     0x024
#define DISP_BL_GREEN   0x028
#define DISP_BL_BLUE    0x02C
#define CTRL_UPDATE     (1 << 1)

// Brightness levels for QEMU display grayscale path
#define BACKLIGHT_OFF_LEVEL 180
#define BACKLIGHT_ON_LEVEL  255

#ifdef CONFIG_BACKLIGHT_QEMU_COLOR
static void prv_write_color(uint32_t rgb_color, uint8_t brightness) {
  pbl_writel(((rgb_color >> 16) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX,
             QEMU_DISPLAY_BASE + DISP_BL_RED);
  pbl_writel(((rgb_color >> 8) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX,
             QEMU_DISPLAY_BASE + DISP_BL_GREEN);
  pbl_writel((rgb_color & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX,
             QEMU_DISPLAY_BASE + DISP_BL_BLUE);
}

static int prv_init(const struct pbl_backlight *bl) {
  prv_write_color(BACKLIGHT_COLOR_WHITE, PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  prv_write_color(bl->state->color, brightness);

  return 0;
}

static int prv_set_color(const struct pbl_backlight *bl, uint32_t rgb_color) {
  prv_write_color(rgb_color, bl->state->brightness);

  return 0;
}
#else
static void prv_write_level(uint32_t level) {
  pbl_writel(level, QEMU_DISPLAY_BASE + DISP_BRIGHTNESS);
  pbl_writel(pbl_readl(QEMU_DISPLAY_BASE + DISP_CTRL) | CTRL_UPDATE, QEMU_DISPLAY_BASE + DISP_CTRL);
}

static int prv_init(const struct pbl_backlight *bl) {
  prv_write_level(BACKLIGHT_OFF_LEVEL);

  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  prv_write_level(BACKLIGHT_OFF_LEVEL +
                  ((uint32_t)brightness * (BACKLIGHT_ON_LEVEL - BACKLIGHT_OFF_LEVEL)) /
                      PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return 0;
}
#endif

static const struct pbl_backlight_ops s_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
#ifdef CONFIG_BACKLIGHT_QEMU_COLOR
  .set_color = prv_set_color,
#endif
};

PBL_BACKLIGHT_STATE_DEFINE(pbl_backlight_qemu);
const struct pbl_backlight pbl_backlight_qemu =
    PBL_BACKLIGHT_INIT(pbl_backlight_qemu, "backlight", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_backlight_qemu, &pbl_backlight_qemu.dev);
