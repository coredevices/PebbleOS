/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_sdl_bottom.h"

#include <errno.h>
#include <string.h>

#include <pbl/drivers/display/sdl.h>

#include <board/display.h>

static uint8_t s_fb[PBL_DISPLAY_WIDTH * PBL_DISPLAY_HEIGHT];
static bool s_enabled;

static void prv_flush(const struct pbl_display *disp) {
  if (s_enabled) {
    display_sdl_bottom_update(s_fb, disp->width, disp->height);
  }
}

static int prv_init(const struct pbl_display *disp) {
  if ((uint32_t)disp->width * disp->height > sizeof(s_fb)) {
    return -EINVAL;
  }

  s_enabled = true;

  return 0;
}

static int prv_enable(const struct pbl_display *disp) {
  s_enabled = true;

  return 0;
}

static int prv_disable(const struct pbl_display *disp) {
  s_enabled = false;

  return 0;
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  for (uint16_t y = y0; y <= y1; y++) {
    const uint8_t *src = &buf->data[y * buf->pitch];
    uint8_t *dst = &s_fb[y * disp->width];

    if (disp->format == PBL_DISPLAY_FORMAT_MONO) {
      for (uint16_t x = 0U; x < disp->width; x++) {
        dst[x] = (src[x / 8U] & (1U << (x % 8U))) != 0U ? 0xffU : 0xc0U;
      }
    } else {
      memcpy(dst, src, disp->width);
    }
  }

  prv_flush(disp);
  pbl_display_complete(disp, 0);

  return 0;
}

static int prv_clear(const struct pbl_display *disp) {
  memset(s_fb, 0, sizeof(s_fb));
  prv_flush(disp);

  return 0;
}

const struct pbl_display_ops pbl_display_sdl_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .clear = prv_clear,
};
