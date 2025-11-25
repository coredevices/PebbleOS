/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"

#include "applib/graphics/gtypes.h"
#include "board/display.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/profiler.h"
#include "util/bitset.h"

#include <stdint.h>
#include <string.h>

volatile const int FrameBuffer_MaxX = DISP_COLS;
volatile const int FrameBuffer_MaxY = DISP_ROWS;
volatile const int FrameBuffer_BytesPerRow = FRAMEBUFFER_BYTES_PER_ROW;

uint8_t *framebuffer_get_line(FrameBuffer *f, uint8_t y) {
  PBL_ASSERTN(!gsize_equal(&f->size, &GSizeZero));
  PBL_ASSERTN(y < f->size.h);

#if PLATFORM_SPALDING
  const GBitmapDataRowInfoInternal *row_infos = g_gbitmap_spalding_data_row_infos;
  const size_t offset = row_infos[y].offset;
#else
  const size_t offset = y * f->size.w;
#endif
  return f->buffer + offset;
}

inline size_t framebuffer_get_size_bytes(FrameBuffer *f) {
  PBL_ASSERTN(!gsize_equal(&f->size, &GSizeZero));
  // TODO: Make FRAMEBUFFER_SIZE_BYTES a macro which takes the cols and rows if we ever want
  // to support different size framebuffers for round displays or other displays where the 8-bit
  // framebuffer size is not just COLS * ROWS.
#if PLATFORM_SPALDING
  return FRAMEBUFFER_SIZE_BYTES;
#else
  return (size_t)f->size.w * (size_t)f->size.h;
#endif
}

void framebuffer_clear(FrameBuffer *f) {
  PBL_ASSERTN(!gsize_equal(&f->size, &GSizeZero));
  memset(f->buffer, 0xff, framebuffer_get_size_bytes(f));
  framebuffer_dirty_all(f);
}

void framebuffer_mark_dirty_rect(FrameBuffer *f, GRect rect) {
  PBL_ASSERTN(!gsize_equal(&f->size, &GSizeZero));
  if (!f->is_dirty) {
    f->dirty_rect = rect;
  } else {
    f->dirty_rect = grect_union(&f->dirty_rect, &rect);
  }

  const GRect clip_rect = (GRect) { GPointZero, f->size };
  grect_clip(&f->dirty_rect, &clip_rect);

  f->is_dirty = true;
}
