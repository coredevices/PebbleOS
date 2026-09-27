/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {
}

void PBL_WEAK graphics_fill_rect(GContext *ctx, const GRect *rect) {
}

void PBL_WEAK graphics_fill_round_rect(GContext *ctx, const GRect *rect, uint16_t radius,
                                       GCornerMask corner_mask) {
}

void PBL_WEAK graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) {
}

void PBL_WEAK graphics_draw_bitmap_in_rect_processed(GContext *ctx, const GBitmap *src_bitmap,
                                                     const GRect *rect,
                                                     GBitmapProcessor *processor) {
}

void PBL_WEAK graphics_draw_text(GContext *ctx, const char *text, GFont const font, GRect box,
                                 const GTextOverflowMode overflow_mode,
                                 const GTextAlignment alignment, GTextLayoutCacheRef const layout) {
}

GBitmap *PBL_WEAK graphics_capture_frame_buffer(GContext *ctx) {
  return NULL;
}

bool PBL_WEAK graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  return true;
}
