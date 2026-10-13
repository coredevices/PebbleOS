/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics.h>

PBL_WEAK void graphics_context_init(GContext *ctx, FrameBuffer *framebuffer,
                                    GContextInitializationMode init_mode) {
  if (ctx) {
    ctx->draw_state = (GDrawState){
      .stroke_color = GColorBlack,
      .fill_color = GColorBlack,
      .text_color = GColorWhite,
      .compositing_mode = GCompOpAssign,
      .stroke_width = 1
    };
  }
}

PBL_WEAK GContext *graphics_context_get_current_context(void) {
  return nullptr;
}

PBL_WEAK void graphics_context_set_antialiased(GContext *ctx, bool enable) {
#if PBL_COLOR
  ctx->draw_state.antialiased = enable;
#endif
}

PBL_WEAK bool graphics_context_get_antialiased(GContext *ctx) {
  return PBL_IF_COLOR_ELSE(ctx->draw_state.antialiased, false);
}

PBL_WEAK void graphics_context_set_stroke_color(GContext *ctx, GColor color) {
}

PBL_WEAK void graphics_context_set_fill_color(GContext *ctx, GColor color) {
}

PBL_WEAK void graphics_context_set_text_color(GContext *ctx, GColor color) {
}

PBL_WEAK void graphics_context_set_stroke_width(GContext *ctx, uint8_t stroke_width) {
}

PBL_WEAK void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode) {
  if (ctx) {
    ctx->draw_state.compositing_mode = mode;
  }
}

PBL_WEAK GBitmap *graphics_context_get_bitmap(GContext *ctx) {
  return nullptr;
}

PBL_WEAK void graphics_context_mark_dirty_rect(GContext *ctx, GRect rect) {
}

PBL_WEAK GSize graphics_context_get_framebuffer_size(GContext *ctx) {
  return GSize(DISP_COLS, DISP_ROWS);
}
