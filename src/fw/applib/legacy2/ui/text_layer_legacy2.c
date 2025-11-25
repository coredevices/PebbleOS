/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_layer_legacy2.h"

#include "applib/graphics/gtypes.h"
#include "process_state/app_state/app_state.h"
#include "applib/graphics/graphics.h"
#include "applib/fonts/fonts.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/passert.h"

#include <string.h>
#include <stddef.h>

static GTextLayoutCacheRef prv_text_layer_legacy2_get_cache_handle(TextLayerLegacy2 *text_layer) {
  if (text_layer == NULL) {
    return NULL;
  }
  return text_layer->should_cache_layout ? text_layer->layout_cache : NULL;
}

void text_layer_legacy2_update_proc(TextLayerLegacy2 *text_layer, GContext* ctx) {
  if (text_layer == NULL) {
    return;
  }
  const GColor bg_color = get_native_color(text_layer->background_color);
  if (!(gcolor_equal(bg_color, GColorClear))) {
    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_rect(ctx, &text_layer->layer.bounds);
  }
  if (text_layer->text && strlen(text_layer->text) > 0) {
    graphics_context_set_text_color(ctx, get_native_color(text_layer->text_color));
    graphics_draw_text(ctx, text_layer->text, text_layer->font, text_layer->layer.bounds,
                       text_layer->overflow_mode, text_layer->text_alignment,
                       prv_text_layer_legacy2_get_cache_handle(text_layer));
  }
}

void text_layer_legacy2_init(TextLayerLegacy2 *text_layer, const GRect *frame) {
  PBL_ASSERTN(text_layer);
  *text_layer = (TextLayerLegacy2){};
  text_layer->layer.frame = *frame;
  text_layer->layer.bounds = (GRect){{0, 0}, frame->size};
  text_layer->layer.update_proc = (LayerUpdateProc)text_layer_legacy2_update_proc;
  text_layer->text_color = GColor2Black;
  text_layer->background_color = GColor2White;
  text_layer->overflow_mode = GTextOverflowModeTrailingEllipsis;
  layer_set_clips(&text_layer->layer, true);

  text_layer->text_alignment = GTextAlignmentLeft;
  text_layer->font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  layer_mark_dirty(&(text_layer->layer));
}

TextLayerLegacy2* text_layer_legacy2_create(GRect frame) {
  TextLayerLegacy2* layer = task_malloc(sizeof(TextLayerLegacy2));
  if (layer) {
    text_layer_legacy2_init(layer, &frame);
  }
  return layer;
}

void text_layer_legacy2_destroy(TextLayerLegacy2* text_layer) {
  if (!text_layer) {
    return;
  }
  text_layer_legacy2_deinit(text_layer);
  task_free(text_layer);
}

void text_layer_legacy2_deinit(TextLayerLegacy2 *text_layer) {
  if (text_layer == NULL) {
    return;
  }
  layer_deinit(&text_layer->layer);
  graphics_text_layout_cache_deinit(&text_layer->layout_cache);
  text_layer->layout_cache = NULL;
}

Layer* text_layer_legacy2_get_layer(TextLayerLegacy2 *text_layer) {
  if (text_layer == NULL) {
    return NULL;
  }
  return &text_layer->layer;
}

void text_layer_legacy2_set_size(TextLayerLegacy2 *text_layer, const GSize max_size) {
  if (text_layer == NULL) {
    return;
  }
  layer_set_frame(&text_layer->layer, &(GRect)  { text_layer->layer.frame.origin, max_size });
  layer_mark_dirty(&text_layer->layer);
}

GSize text_layer_legacy2_get_size(TextLayerLegacy2* text_layer) {
  if (text_layer == NULL) {
    return GSizeZero;
  }
  return text_layer->layer.frame.size;
}

void text_layer_legacy2_set_text(TextLayerLegacy2 *text_layer, const char *text) {
  if (text_layer == NULL) {
    return;
  }
  text_layer->text = text;
  layer_mark_dirty(&text_layer->layer);
}

const char* text_layer_legacy2_get_text(TextLayerLegacy2 *text_layer) {
  if (text_layer == NULL) {
    return NULL;
  }
  return text_layer->text;
}

void text_layer_legacy2_set_background_color_2bit(TextLayerLegacy2 *text_layer, GColor2 color) {
  if (text_layer == NULL) {
    return;
  }
  GColor native_color = get_native_color(color);
  const GColor bg_color = get_native_color(text_layer->background_color);
  if (gcolor_equal(native_color, bg_color)) {
    return;
  }
  text_layer->background_color = get_closest_gcolor2(native_color);
  layer_mark_dirty(&(text_layer->layer));
}

void text_layer_legacy2_set_text_color_2bit(TextLayerLegacy2 *text_layer, GColor2 color) {
  if (text_layer == NULL) {
    return;
  }
  GColor8 native_color = get_native_color(color);
  const GColor text_color = get_native_color(text_layer->text_color);
  if (gcolor_equal(native_color, text_color)) {
    return;
  }
  text_layer->text_color = get_closest_gcolor2(native_color);
  layer_mark_dirty(&(text_layer->layer));
}

void text_layer_legacy2_set_text_alignment(TextLayerLegacy2 *text_layer,
                                           GTextAlignment text_alignment) {
  if (text_layer == NULL || text_alignment == text_layer->text_alignment) {
    return;
  }
  text_layer->text_alignment = text_alignment;
  layer_mark_dirty(&(text_layer->layer));
}

void text_layer_legacy2_set_overflow_mode(TextLayerLegacy2 *text_layer,
                                          GTextOverflowMode overflow_mode) {
  if (text_layer == NULL || overflow_mode == text_layer->overflow_mode) {
    return;
  }
  text_layer->overflow_mode = overflow_mode;
  layer_mark_dirty(&(text_layer->layer));
}

void text_layer_legacy2_set_font(TextLayerLegacy2 *text_layer, GFont font) {
  if (text_layer == NULL || font == text_layer->font) {
    return;
  }
  text_layer->font = font;
  layer_mark_dirty(&(text_layer->layer));
}

void text_layer_legacy2_set_should_cache_layout(TextLayerLegacy2 *text_layer,
                                                bool should_cache_layout) {
  if (text_layer == NULL || should_cache_layout == text_layer->should_cache_layout) {
    return;
  }

  text_layer->should_cache_layout = should_cache_layout;

  if (text_layer->should_cache_layout) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Init layout");
    graphics_text_layout_cache_init(&text_layer->layout_cache);
  } else {
    graphics_text_layout_cache_deinit(&text_layer->layout_cache);
    text_layer->layout_cache = NULL;
  }
}

GSize text_layer_legacy2_get_content_size(GContext* ctx, TextLayerLegacy2 *text_layer) {
  if (text_layer == NULL) {
    return GSizeZero;
  } else if (!text_layer->should_cache_layout) {
    text_layer_legacy2_set_should_cache_layout(text_layer, true);
  }
  GTextLayoutCacheRef layout = prv_text_layer_legacy2_get_cache_handle(text_layer);
  PBL_ASSERTN(layout);
  return graphics_text_layout_get_max_used_size(ctx, text_layer->text, text_layer->font,
      text_layer->layer.bounds, text_layer->overflow_mode, text_layer->text_alignment, layout);
}

GSize app_text_layer_legacy2_get_content_size(TextLayerLegacy2 *text_layer) {
  GContext* ctx = app_state_get_graphics_context();
  return text_layer_legacy2_get_content_size(ctx, text_layer);
}
