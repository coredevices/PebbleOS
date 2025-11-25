/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "compositor.h"

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gtypes.h"
#include "util/bitset.h"
#include "util/math.h"
#include "util/size.h"

#include <string.h>

//! This variable is used when we are flushing s_framebuffer out to the display driver.
//! It's set to the current row index that we are DMA'ing out to the display.
static uint8_t s_current_flush_line;

static void (*s_update_complete_handler)(void);

#if PLATFORM_SILK || PLATFORM_ASTERIX
static const uint8_t s_corner_shape[] = { 3, 1, 1 };
static uint8_t s_line_buffer[FRAMEBUFFER_BYTES_PER_ROW];
#endif

//! display_update get next line callback
static bool prv_flush_get_next_line_cb(DisplayRow* row) {
  FrameBuffer *fb = compositor_get_framebuffer();

  s_current_flush_line = MAX(s_current_flush_line, fb->dirty_rect.origin.y);
  const uint8_t y_end = fb->dirty_rect.origin.y + fb->dirty_rect.size.h;
  if (s_current_flush_line < y_end) {
    row->address = s_current_flush_line;
    void *fb_line = framebuffer_get_line(fb, s_current_flush_line);
#if PLATFORM_SILK || PLATFORM_ASTERIX
    // Draw rounded corners onto the screen without modifying the
    // system framebuffer.
    if (s_current_flush_line < ARRAY_LENGTH(s_corner_shape) ||
        s_current_flush_line >= DISP_ROWS - ARRAY_LENGTH(s_corner_shape)) {
      memcpy(s_line_buffer, fb_line, FRAMEBUFFER_BYTES_PER_ROW);
      uint8_t corner_idx =
        (s_current_flush_line < ARRAY_LENGTH(s_corner_shape))?
        s_current_flush_line : DISP_ROWS - s_current_flush_line - 1;
      uint8_t corner_width = s_corner_shape[corner_idx];
      for (uint8_t pixel = 0; pixel < corner_width; ++pixel) {
        bitset8_clear(s_line_buffer, pixel);
        bitset8_clear(s_line_buffer, DISP_COLS - pixel - 1);
      }
      row->data = s_line_buffer;
    } else {
      row->data = fb_line;
    }
#else
    row->data = fb_line;
#endif
    s_current_flush_line++;
    return true;
  }

  return false;
}

//! display_update complete callback
static void prv_flush_complete_cb(void) {
  s_current_flush_line = 0;
  framebuffer_reset_dirty(compositor_get_framebuffer());

  if (s_update_complete_handler) {
    s_update_complete_handler();
  }
}

void compositor_display_update(void (*handle_update_complete_cb)(void)) {
  if (!framebuffer_is_dirty(compositor_get_framebuffer())) {
    return;
  }
  s_update_complete_handler = handle_update_complete_cb;
  s_current_flush_line = 0;

  display_update(&prv_flush_get_next_line_cb, &prv_flush_complete_cb);
}

bool compositor_display_update_in_progress(void) {
  return display_update_in_progress();
}

