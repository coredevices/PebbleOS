/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "logo.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/display.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#ifdef CONFIG_TOUCH
#include <pbl/drivers/touch.h>
#endif

#define FRAME_MS 16U

/* Brightness changes by its whole range over a drag of the display height */
#define BRIGHTNESS_MIN 5

/* A tap: short, and staying where it landed */
#define TAP_MAX_MS   400U
#define TAP_MAX_MOVE 20

/* Speed, in percent of the display size per second */
#define SPEED_X 40
#define SPEED_Y 24

/* The logo color, #fa4a36 */
static const uint8_t s_logo_rgb[3] = {0xFA, 0x4A, 0x36};

struct demo {
  const struct pbl_display_props *props;
  struct pbl_display_buffer fb;
  /* Where the logo bounces: the square inside a round display */
  uint16_t area_x;
  uint16_t area_y;
  uint16_t area_w;
  uint16_t area_h;
  /* The logo scaled to the display, as the coverage of each pixel */
  uint8_t *logo;
  uint16_t logo_w;
  uint16_t logo_h;
  bool flipped;
  uint8_t brightness;
};

/* The logo color over black, by the coverage of the pixel */
static void prv_put(struct demo *d, uint16_t x, uint16_t y, uint32_t a) {
  uint8_t *row = &d->fb.data[y * d->fb.pitch];
  uint32_t r = s_logo_rgb[0] * a / 255U;
  uint32_t g = s_logo_rgb[1] * a / 255U;
  uint32_t b = s_logo_rgb[2] * a / 255U;

  switch (d->props->format) {
    case PBL_DISPLAY_FORMAT_RGB565:
      ((uint16_t *)row)[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      break;
    case PBL_DISPLAY_FORMAT_ARGB2222:
      row[x] = (uint8_t)(0xC0U | ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6));
      break;
    default:
      if (a >= 128U) {
        row[x / 8U] |= (uint8_t)(1U << (x % 8U));
      } else {
        row[x / 8U] &= (uint8_t)~(1U << (x % 8U));
      }
      break;
  }
}

static void prv_fill(struct demo *d, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h) {
  for (uint16_t y = y0; y < y0 + h; y++) {
    for (uint16_t x = x0; x < x0 + w; x++) {
      prv_put(d, x, y, 0U);
    }
  }
}

static void prv_draw_logo(struct demo *d, uint16_t x0, uint16_t y0) {
  for (uint16_t y = 0; y < d->logo_h; y++) {
    uint16_t sy = d->flipped ? d->logo_h - 1U - y : y;

    for (uint16_t x = 0; x < d->logo_w; x++) {
      uint16_t sx = d->flipped ? d->logo_w - 1U - x : x;

      prv_put(d, x0 + x, y0 + y, d->logo[sy * d->logo_w + sx]);
    }
  }
}

/* Two thirds of the area wide at most, each pixel the average of those it covers */
static int prv_scale_logo(struct demo *d) {
  uint32_t w = d->area_w * 2U / 3U;

  if (w > LOGO_WIDTH) {
    w = LOGO_WIDTH;
  }

  d->logo_w = (uint16_t)w;
  d->logo_h = (uint16_t)(LOGO_HEIGHT * w / LOGO_WIDTH);
  d->logo = pbl_malloc(d->logo_w * d->logo_h);
  if (d->logo == nullptr) {
    return -ENOMEM;
  }

  for (uint32_t y = 0; y < d->logo_h; y++) {
    uint32_t sy0 = y * LOGO_HEIGHT / d->logo_h;
    uint32_t sy1 = (y + 1U) * LOGO_HEIGHT / d->logo_h;

    for (uint32_t x = 0; x < d->logo_w; x++) {
      uint32_t sx0 = x * LOGO_WIDTH / d->logo_w;
      uint32_t sx1 = (x + 1U) * LOGO_WIDTH / d->logo_w;
      uint32_t sum = 0U;

      for (uint32_t sy = sy0; sy < sy1; sy++) {
        for (uint32_t sx = sx0; sx < sx1; sx++) {
          sum += s_logo[sy * LOGO_WIDTH + sx];
        }
      }

      d->logo[y * d->logo_w + x] = (uint8_t)(sum / ((sy1 - sy0) * (sx1 - sx0)));
    }
  }

  return 0;
}

/* The frame the board reserves for the display, if any, or one from the heap */
static int prv_get_framebuffer(struct demo *d) {
  if (pbl_display_get_framebuffer(DISPLAY, &d->fb) == 0) {
    return 0;
  }

  switch (d->props->format) {
    case PBL_DISPLAY_FORMAT_RGB565:
      d->fb.pitch = d->props->width * 2U;
      break;
    case PBL_DISPLAY_FORMAT_ARGB2222:
      d->fb.pitch = d->props->width;
      break;
    default:
      d->fb.pitch = (d->props->width + 7U) / 8U;
      break;
  }

  d->fb.data = pbl_malloc(d->fb.pitch * d->props->height);

  return d->fb.data != nullptr ? 0 : -ENOMEM;
}

#ifdef CONFIG_TOUCH
static void prv_set_brightness(struct demo *d, int32_t brightness) {
  if (brightness < BRIGHTNESS_MIN) {
    brightness = BRIGHTNESS_MIN;
  } else if (brightness > (int32_t)PBL_BACKLIGHT_BRIGHTNESS_MAX) {
    brightness = PBL_BACKLIGHT_BRIGHTNESS_MAX;
  }

  if (brightness != d->brightness) {
    d->brightness = (uint8_t)brightness;
    (void)pbl_backlight_set_brightness(BACKLIGHT, d->brightness);
  }
}

/* A drag up brightens and down dims, a tap turns the logo upside down: true if it did */
static bool prv_touch(struct demo *d, uint64_t now_ms) {
  static struct pbl_touch_sample prev;
  static uint8_t down_brightness;
  static uint16_t down_x;
  static uint16_t down_y;
  static uint64_t down_ms;
  struct pbl_touch_sample now;
  bool tapped = false;

  pbl_touch_get_last(TOUCH, &now);
  if (now.down && !prev.down) {
    down_brightness = d->brightness;
    down_x = now.x;
    down_y = now.y;
    down_ms = now_ms;
  }

  if (now.down) {
    int32_t moved = (int32_t)down_y - (int32_t)now.y;

    prv_set_brightness(d, down_brightness + moved * (int32_t)PBL_BACKLIGHT_BRIGHTNESS_MAX /
                                                (int32_t)d->props->height);
  } else if (prev.down && now_ms - down_ms <= TAP_MAX_MS &&
             abs((int)prev.x - (int)down_x) <= TAP_MAX_MOVE &&
             abs((int)prev.y - (int)down_y) <= TAP_MAX_MOVE) {
    d->flipped = !d->flipped;
    tapped = true;
    printf("logo: %s\n", d->flipped ? "upside down" : "upright");
  }
  prev = now;

  return tapped;
}
#endif

int main(void) {
  struct demo d = {.props = pbl_display_get_props(DISPLAY)};
  int32_t px = 0;
  int32_t py = 0;
  int32_t vx;
  int32_t vy;
  uint64_t last_ms;
  uint16_t lx;
  uint16_t ly;
  bool redraw = true;
  int ret;

  d.area_w = d.props->width;
  d.area_h = d.props->height;
  if (d.props->round_mask != nullptr) {
    /* The side of the square inside the circle: the diameter over the square root of 2 */
    d.area_w = (uint16_t)(d.props->width * 707U / 1000U);
    d.area_h = d.area_w;
  }
  d.area_x = (d.props->width - d.area_w) / 2U;
  d.area_y = (d.props->height - d.area_h) / 2U;
  lx = d.area_x;
  ly = d.area_y;

  ret = prv_scale_logo(&d);
  if (ret == 0) {
    ret = prv_get_framebuffer(&d);
  }
  if (ret != 0) {
    printf("logo: no memory (%d)\n", ret);
    return ret;
  }

  d.brightness = PBL_BACKLIGHT_BRIGHTNESS_MAX;
  (void)pbl_backlight_set_brightness(BACKLIGHT, d.brightness);
  prv_fill(&d, 0U, 0U, d.props->width, d.props->height);
  ret = pbl_display_update_sync(DISPLAY, &d.fb, 0U, d.props->height - 1U);
  if (ret != 0) {
    printf("logo: display update failed (%d)\n", ret);
    return ret;
  }

#ifdef CONFIG_TOUCH
  ret = pbl_touch_enable(TOUCH);
  if (ret != 0) {
    printf("logo: touch enable failed (%d)\n", ret);
  }
#endif

  printf("logo: %ux%u, bouncing\n", d.props->width, d.props->height);

  /* In 1/256 pixels per second */
  vx = (int32_t)d.props->width * 256 * SPEED_X / 100;
  vy = (int32_t)d.props->height * 256 * SPEED_Y / 100;
  last_ms = pbl_uptime_ms();

  while (true) {
    uint64_t start = pbl_uptime_ms();

#ifdef CONFIG_TOUCH
    redraw |= prv_touch(&d, start);
#endif

    /* Bounce off the edges of the area */
    int32_t max_x = (int32_t)(d.area_w - d.logo_w) * 256;
    int32_t max_y = (int32_t)(d.area_h - d.logo_h) * 256;

    /* In 1/256 pixels, by the time since the last frame */
    px += vx * (int32_t)(start - last_ms) / 1000;
    py += vy * (int32_t)(start - last_ms) / 1000;
    last_ms = start;
    if (px < 0 || px > max_x) {
      vx = -vx;
      px = px < 0 ? -px : 2 * max_x - px;
    }
    if (py < 0 || py > max_y) {
      vy = -vy;
      py = py < 0 ? -py : 2 * max_y - py;
    }
    /* A long stall may overshoot the bounce */
    px = px < 0 ? 0 : (px > max_x ? max_x : px);
    py = py < 0 ? 0 : (py > max_y ? max_y : py);

    uint16_t x = d.area_x + (uint16_t)(px / 256);
    uint16_t y = d.area_y + (uint16_t)(py / 256);

    if (redraw || x != lx || y != ly) {
      uint16_t y0 = y < ly ? y : ly;
      uint16_t y1 = (y > ly ? y : ly) + d.logo_h - 1U;

      prv_fill(&d, lx, ly, d.logo_w, d.logo_h);
      prv_draw_logo(&d, x, y);
      (void)pbl_display_update_sync(DISPLAY, &d.fb, y0, y1);
      lx = x;
      ly = y;
      redraw = false;
    }

    uint64_t spent = pbl_uptime_ms() - start;
    if (spent < FRAME_MS) {
      pbl_sleep_ms((uint32_t)(FRAME_MS - spent));
    }
  }

  return 0;
}
