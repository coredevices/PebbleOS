/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/display.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/sem.h>

#define BARS    8
#define SQUARE  16
#define SHOW_MS 3000

/* ARGB2222, opaque: red, green and blue from 0 to 3. */
#define RGB(r, g, b) (0xC0U | ((r) << 4) | ((g) << 2) | (b))

static const uint8_t s_bar_colors[BARS] = {
  RGB(3, 3, 3), RGB(3, 3, 0), RGB(0, 3, 3), RGB(0, 3, 0),
  RGB(3, 0, 3), RGB(3, 0, 0), RGB(0, 0, 3), RGB(0, 0, 0),
};

static PBL_SEM_DEFINE(s_done, 0, 1);
static int s_status;

static void prv_set_pixel(const struct pbl_display_props *props,
                          const struct pbl_display_buffer *fb, uint16_t x, uint16_t y,
                          uint8_t color) {
  uint8_t *row = &fb->data[y * fb->pitch];

  if (props->format == PBL_DISPLAY_FORMAT_ARGB2222) {
    row[x] = color;
  } else if (props->format == PBL_DISPLAY_FORMAT_RGB565) {
    /* Each 2-bit channel scaled to 5, 6 and 5 bits. */
    uint16_t r = ((color >> 4) & 3U) * 31U / 3U;
    uint16_t g = ((color >> 2) & 3U) * 63U / 3U;
    uint16_t b = (color & 3U) * 31U / 3U;

    ((uint16_t *)row)[x] = (uint16_t)((r << 11) | (g << 5) | b);
  } else if ((color & 0x3FU) != 0U) {
    /* Mono: anything but black is white, the leftmost pixel in the lowest bit. */
    row[x / 8U] |= (uint8_t)(1U << (x % 8U));
  } else {
    row[x / 8U] &= (uint8_t)~(1U << (x % 8U));
  }
}

static void prv_draw_bars(const struct pbl_display_props *props,
                          const struct pbl_display_buffer *fb) {
  for (uint16_t y = 0U; y < props->height; y++) {
    for (uint16_t x = 0U; x < props->width; x++) {
      prv_set_pixel(props, fb, x, y, s_bar_colors[x * BARS / props->width]);
    }
  }
}

static void prv_draw_checkerboard(const struct pbl_display_props *props,
                                  const struct pbl_display_buffer *fb, uint16_t y0, uint16_t y1) {
  for (uint16_t y = y0; y <= y1; y++) {
    for (uint16_t x = 0U; x < props->width; x++) {
      prv_set_pixel(props, fb, x, y,
                    ((x / SQUARE + y / SQUARE) % 2U) != 0U ? RGB(3, 3, 3) : RGB(0, 0, 0));
    }
  }
}

/* Runs from the display's interrupt: only hand the result over. */
static void prv_update_done(int status, void *arg) {
  s_status = status;
  pbl_sem_give(&s_done);
}

int main(void) {
  /* Size and pixel format: what a framebuffer for this display looks like. */
  const struct pbl_display_props *props = pbl_display_get_props(DISPLAY);
  static const char *const formats[] = {
    [PBL_DISPLAY_FORMAT_MONO] = "mono",
    [PBL_DISPLAY_FORMAT_ARGB2222] = "argb2222",
    [PBL_DISPLAY_FORMAT_RGB565] = "rgb565",
  };
  uint16_t y0 = props->height * 2U / 3U;
  uint16_t y1 = props->height - 1U;
  struct pbl_display_buffer fb;
  int ret;

  printf("display: %dx%d, %s\n", props->width, props->height, formats[props->format]);

  /* The buffer is the whole frame: updates send rows of it, by their position on the display. */
  /* The frame the board reserves for the display, if any, or one from the heap. */
  bool reserved = pbl_display_get_framebuffer(DISPLAY, &fb) == 0;
  if (!reserved) {
    fb.pitch = props->format == PBL_DISPLAY_FORMAT_MONO     ? (props->width + 7U) / 8U
               : props->format == PBL_DISPLAY_FORMAT_RGB565 ? props->width * 2U
                                                            : props->width;
    fb.data = pbl_zalloc(fb.pitch * props->height);
    if (fb.data == nullptr) {
      fprintf(stderr, "display: no memory for a frame\n");
      return -ENOMEM;
    }
  }

  prv_draw_bars(props, &fb);
  ret = pbl_display_update_sync(DISPLAY, &fb, 0U, props->height - 1U);
  if (ret != 0) {
    fprintf(stderr, "display: update failed (%d)\n", ret);
    goto out;
  }

  printf("display: bars shown\n");
  pbl_sleep_ms(SHOW_MS);

  /* Only the rows that changed, in the background: the callback says when they are out. */
  prv_draw_checkerboard(props, &fb, y0, y1);
  ret = pbl_display_update(DISPLAY, &fb, y0, y1, prv_update_done, nullptr);
  if (ret == 0) {
    (void)pbl_sem_take(&s_done, PBL_FOREVER);
    ret = s_status;
  }
  if (ret != 0) {
    fprintf(stderr, "display: update failed (%d)\n", ret);
    goto out;
  }

  printf("display: checkerboard shown\n");
  pbl_sleep_ms(SHOW_MS);

  printf("Sample done\n");

out:
  if (!reserved) {
    pbl_free(fb.data);
  }

  return ret;
}
