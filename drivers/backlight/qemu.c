/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/qemu.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

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

static uintptr_t prv_base(const struct pbl_backlight *bl) {
  return container_of(bl, const struct pbl_backlight_qemu, bl)->base;
}

#ifdef CONFIG_BACKLIGHT_QEMU_COLOR
static void prv_write_color(uintptr_t base, uint32_t rgb_color, uint8_t brightness) {
  pbl_writel(((rgb_color >> 16) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX,
             base + DISP_BL_RED);
  pbl_writel(((rgb_color >> 8) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX,
             base + DISP_BL_GREEN);
  pbl_writel((rgb_color & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX, base + DISP_BL_BLUE);
}

static int prv_init(const struct pbl_backlight *bl) {
  prv_write_color(prv_base(bl), BACKLIGHT_COLOR_WHITE, PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  prv_write_color(prv_base(bl), bl->state->color, brightness);

  return 0;
}

static int prv_set_color(const struct pbl_backlight *bl, uint32_t rgb_color) {
  prv_write_color(prv_base(bl), rgb_color, bl->state->brightness);

  return 0;
}
#else
static void prv_write_level(uintptr_t base, uint32_t level) {
  pbl_writel(level, base + DISP_BRIGHTNESS);
  pbl_writel(pbl_readl(base + DISP_CTRL) | CTRL_UPDATE, base + DISP_CTRL);
}

static int prv_init(const struct pbl_backlight *bl) {
  prv_write_level(prv_base(bl), BACKLIGHT_OFF_LEVEL);

  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  prv_write_level(prv_base(bl), BACKLIGHT_OFF_LEVEL + ((uint32_t)brightness *
                                                       (BACKLIGHT_ON_LEVEL - BACKLIGHT_OFF_LEVEL)) /
                                                          PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return 0;
}
#endif

const struct pbl_backlight_ops pbl_backlight_qemu_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
#ifdef CONFIG_BACKLIGHT_QEMU_COLOR
  .set_color = prv_set_color,
#endif
};

#include PBL_DT_INSTANCES(qemu_pebble_display_backlight)
