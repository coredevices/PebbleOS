/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics.h"

void graphics_context_init(GContext *ctx, FrameBuffer *framebuffer,
                           GContextInitializationMode init_mode) {
  if (ctx) {
    ctx->draw_state = (GDrawState) {
      .stroke_color = GColorBlack,
      .fill_color = GColorBlack,
      .text_color = GColorWhite,
      .compositing_mode = GCompOpAssign,
      .stroke_width = 1
    };
  }
}

GContext *graphics_context_get_current_context(void) {
  return NULL;
}

void graphics_context_set_antialiased(GContext *ctx, bool enable) {
#if PBL_COLOR
  ctx->draw_state.antialiased = enable;
#endif
}

bool graphics_context_get_antialiased(GContext *ctx) {
  return PBL_IF_COLOR_ELSE(ctx->draw_state.antialiased, false);
}

void graphics_context_set_stroke_color(GContext* ctx, GColor color) {}

void graphics_context_set_fill_color(GContext* ctx, GColor color) {}

void graphics_context_set_text_color(GContext* ctx, GColor color) {}

void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {}

void graphics_context_set_compositing_mode(GContext* ctx, GCompOp mode) {
  if (ctx) {
    ctx->draw_state.compositing_mode = mode;
  }
}

GBitmap* graphics_context_get_bitmap(GContext* ctx) {
  return NULL;
}

void graphics_context_mark_dirty_rect(GContext* ctx, GRect rect) {}

GSize graphics_context_get_framebuffer_size(GContext *ctx) {
  return GSize(DISP_COLS, DISP_ROWS);
}
