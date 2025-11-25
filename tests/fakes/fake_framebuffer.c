/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "framebuffer.h"

#include "drivers/display.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/bitset.h"

#include <stdint.h>
#include <string.h>

const int FrameBuffer_MaxX = DISP_COLS;
const int FrameBuffer_MaxY = DISP_ROWS;

static FrameBuffer* s_current_framebuffer;
static uint8_t s_current_flush_line;

static void flush_complete(void);
static bool flush_get_next_line(DisplayRow* row);
static void framebuffer_reset_dirty_lines(FrameBuffer* f);

uint32_t* framebuffer_get_line(FrameBuffer* f, uint8_t y) {
  PBL_ASSERT(y < FrameBuffer_MaxY, "Y coordinate is outside of framebuffer dimensions");

  return f->buffer + (y * FRAMEBUFFER_WORDS_PER_ROW);
}

void framebuffer_clear(FrameBuffer* f) {
  memset(f->buffer, 0xffffffff, FRAMEBUFFER_SIZE_BYTES);
  memset(f->dirty_lines, 0xff, DISP_ROWS / 8);
  f->is_dirty = true;
  f->is_cleared = false;
}

void framebuffer_clear_line(FrameBuffer* f, uint8_t y) {
  uint32_t* line = framebuffer_get_line(f, y);
  memset(line, 0xffffffff, FRAMEBUFFER_SIZE_BYTES);
  bitset8_set(f->dirty_lines, y);

  f->is_dirty = true;
}

void framebuffer_mark_dirty_rect(FrameBuffer* f, GRect rect) {
  const uint16_t y_start = rect.origin.y;
  const uint16_t y_end = y_start + rect.size.h;
  for (uint16_t y = y_start; y < y_end; ++y) {
    bitset8_update(f->dirty_lines, y, is_dirty);
  }

  f->is_dirty = true;
}

void framebuffer_set_line(FrameBuffer* f, uint8_t y, const uint32_t* buffer) {
  memcpy(framebuffer_get_line(f, y), buffer, FRAMEBUFFER_WORDS_PER_ROW);
  bitset8_set(f->dirty_lines, y);

  f->is_dirty = true;
}

void framebuffer_set_lines(FrameBuffer* f, uint8_t y, const uint8_t num_lines, const uint32_t* buffer) {
  uint32_t* line = framebuffer_get_line(f, y);
  memcpy(line, buffer, num_lines * FRAMEBUFFER_WORDS_PER_ROW);
  const GRect dirty_rect = GRect(0, y, DISP_COLS, num_lines);
  framebuffer_mark_dirty_rect(f, dirty_rect);
}

void framebuffer_flush(FrameBuffer* f) {
  // If the framebuffer hasn't been cleared but it is dirty, issue a
  // display command to blank the screen.
  if (f->is_cleared) {
    display_clear();
    f->is_cleared = false;
  }

  if (!f->is_dirty) {
    return;
  }

  s_current_framebuffer = f;
  s_current_flush_line = 0;
  display_update(&flush_get_next_line, &flush_complete);
}

static void flush_complete(void) {
  s_current_flush_line = 0;
  framebuffer_reset_dirty_lines(s_current_framebuffer);
}

static bool flush_get_next_line(DisplayRow* row) {
  while (s_current_flush_line < DISP_ROWS) {
    if (bitset8_get(s_current_framebuffer->dirty_lines, s_current_flush_line)) {
      row->address = s_current_flush_line;
      row->data = framebuffer_get_line(s_current_framebuffer, s_current_flush_line);
      s_current_flush_line++;
      return true;
    }

    s_current_flush_line++;
  }

  return false;
}

static void framebuffer_reset_dirty(FrameBuffer* f) {
  memset(f->dirty_lines, 0x00, DISP_ROWS / 8);
  f->is_dirty = false;
  f->is_cleared = false;
}


