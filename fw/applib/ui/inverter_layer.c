/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "inverter_layer.h"

#include <pbl/util/math.h>

#include <applib/applib_malloc.auto.h>
#include <applib/graphics/graphics.h>
#include <system/passert.h>

#if CONFIG_SCREEN_COLOR_DEPTH_BITS != 1
inline static void prv_inverter_layer_update_proc_color(GContext *ctx) {
  // ctx->draw_state.drawing_box is the correct rect when this function gets
  // called through layer_render_tree(),
  GRect rect = ctx->draw_state.drawing_box;
  // invert bytes in rect
  grect_clip(&rect, &ctx->dest_bitmap.bounds); // clip to display bounds
  for (int16_t y = rect.origin.y; y < rect.origin.y + rect.size.h; y++) {
    const GBitmapDataRowInfo row = gbitmap_get_data_row_info(&ctx->dest_bitmap, y);
    const int16_t x_begin = MAX(rect.origin.x, row.min_x);
    const int16_t x_end = MIN(rect.origin.x + rect.size.w - 1, row.max_x);
    for (int16_t x = x_begin; x <= x_end; x++) {
      uint8_t *pixel_addr = &row.data[x];
      // Only invert the RGB and not the alpha
      *pixel_addr = (~(*pixel_addr) & 0b00111111) | (*pixel_addr & 0b11000000);
    }
  }

  graphics_context_mark_dirty_rect(ctx, ctx->draw_state.drawing_box);
}
#else
inline static void prv_inverter_layer_update_proc_bw(GContext *ctx) {
  // For 1Bit, just revert to the 2.x code.
  GBitmap sub_bitmap;
  GBitmap *context_bitmap = graphics_context_get_bitmap(ctx);
  // ctx->draw_state.drawing_box is the correct rect when this function gets called through
  // layer_render_tree(), although it might be nicer to have a function to map a rect to another
  // coordinate system...
  gbitmap_init_as_sub_bitmap(&sub_bitmap, context_bitmap, ctx->draw_state.drawing_box);

  // The sub-bitmap might have different bounds than this layer:
  // when the requested bounds lie outside of the original bitmap it will be clipped.
  // The following work-around will make sure the sub-bitmap gets painted at
  // exactly the same spot as it came from:
  GRect rect = sub_bitmap.bounds;
  rect.origin.x -= ctx->draw_state.drawing_box.origin.x;
  rect.origin.y -= ctx->draw_state.drawing_box.origin.y;
  graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
  graphics_draw_bitmap_in_rect(ctx, &sub_bitmap, &rect);
}
#endif

void inverter_layer_update_proc(InverterLayer *inverter, GContext *ctx) {
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
  prv_inverter_layer_update_proc_bw(ctx);
#else
  prv_inverter_layer_update_proc_color(ctx);
#endif
  (void)inverter;
}

void inverter_layer_init(InverterLayer *inverter, const GRect *frame) {
  if (inverter == NULL) {
    return;
  }
  *inverter = (InverterLayer){};
  inverter->layer.frame = *frame;
  inverter->layer.bounds = (GRect){{0, 0}, frame->size};
  inverter->layer.update_proc = (LayerUpdateProc)inverter_layer_update_proc;
  layer_set_clips(&inverter->layer, true);
  layer_mark_dirty(&(inverter->layer));
}

InverterLayer *inverter_layer_create(GRect frame) {
  InverterLayer *layer = applib_type_malloc(InverterLayer);
  if (layer) {
    inverter_layer_init(layer, &frame);
  }
  return layer;
}

void inverter_layer_deinit(InverterLayer *inverter_layer) {
  if (inverter_layer == NULL) {
    return;
  }
  layer_deinit(&inverter_layer->layer);
}

void inverter_layer_destroy(InverterLayer *inverter_layer) {
  if (inverter_layer == NULL) {
    return;
  }
  inverter_layer_deinit(inverter_layer);
  applib_free(inverter_layer);
}

Layer *inverter_layer_get_layer(InverterLayer *inverter_layer) {
  if (inverter_layer == NULL) {
    return NULL;
  }
  return &inverter_layer->layer;
}
