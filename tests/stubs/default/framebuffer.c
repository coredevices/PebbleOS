/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/framebuffer.h>

PBL_WEAK volatile const int FrameBuffer_MaxX = DISP_COLS;

PBL_WEAK volatile const int FrameBuffer_MaxY = DISP_ROWS;

void PBL_WEAK framebuffer_mark_dirty_rect(FrameBuffer *f, GRect rect) {
}

void PBL_WEAK framebuffer_init(FrameBuffer *f, const GSize *size) {
  f->size = *size;
}

GSize PBL_WEAK framebuffer_get_size(FrameBuffer *f) {
  return f->size;
}

size_t PBL_WEAK framebuffer_get_size_bytes(FrameBuffer *f) {
  return f->size.w * f->size.h;
}
