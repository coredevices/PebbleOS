/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_sdl_bottom.h"

#include <string.h>

#include <pbl/drivers/display/display.h>
#include <pbl/kernel/assert.h>

#include <board/board.h>

static uint8_t s_fb[PBL_DISPLAY_WIDTH * PBL_DISPLAY_HEIGHT];
static bool s_enabled;

static void prv_flush(void) {
  if (s_enabled) {
    display_sdl_bottom_update(s_fb, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);
  }
}

void display_init(void) {
  s_enabled = true;
}

void display_clear(void) {
  memset(s_fb, 0, sizeof(s_fb));
  prv_flush();
}

void display_set_enabled(bool enabled) {
  s_enabled = enabled;
}

void display_set_rotated(bool rotated) {
}

bool display_update_in_progress(void) {
  return false;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  PBL_ASSERTN(nrcb != nullptr);
  PBL_ASSERTN(uccb != nullptr);

  DisplayRow row;
  while (nrcb(&row)) {
    uint8_t *dst = &s_fb[row.address * PBL_DISPLAY_WIDTH];
#if PBL_BW
    // 1 bit per pixel, white set, to ARGB2222
    for (int x = 0; x < PBL_DISPLAY_WIDTH; x++) {
      dst[x] = (row.data[x / 8] & (1 << (x % 8))) ? 0xff : 0xc0;
    }
#else
    memcpy(dst, row.data, PBL_DISPLAY_WIDTH);
#endif
  }
  prv_flush();
  uccb(true);
}

void display_update_boot_frame(uint8_t *framebuffer) {
  memcpy(s_fb, framebuffer, sizeof(s_fb));
  prv_flush();
}
