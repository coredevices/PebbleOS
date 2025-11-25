/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "path_layer.h"
#include "applib/graphics/graphics.h"

void path_layer_update_proc(PathLayer *path_layer, GContext* ctx) {
  if (!gcolor_is_transparent(path_layer->fill_color)) {
    graphics_context_set_fill_color(ctx, path_layer->fill_color);
    gpath_draw_filled(ctx, &path_layer->path);
  }
  if (!gcolor_is_transparent(path_layer->stroke_color)) {
    graphics_context_set_stroke_color(ctx, path_layer->stroke_color);
    gpath_draw_outline(ctx, &path_layer->path);
  }
}

void path_layer_init(PathLayer *path_layer, const GPathInfo *path_info) {
  gpath_init(&path_layer->path, path_info);
  const GRect outer_rect = gpath_outer_rect(&path_layer->path);
  layer_init(&path_layer->layer, &outer_rect);
  path_layer->stroke_color = GColorWhite;
  path_layer->fill_color = GColorBlack;
  path_layer->layer.update_proc = (LayerUpdateProc)path_layer_update_proc;
}

void path_layer_deinit(PathLayer *path_layer, const GPathInfo *path_info) {
  layer_deinit(&path_layer->layer);
}

void path_layer_set_stroke_color(PathLayer *path_layer, GColor color) {
  if (gcolor_equal(color, path_layer->stroke_color)) {
    return;
  }
  path_layer->stroke_color = color;
  layer_mark_dirty(&(path_layer->layer));
}

void path_layer_set_fill_color(PathLayer *path_layer, GColor color) {
  if (gcolor_equal(color, path_layer->fill_color)) {
    return;
  }
  path_layer->fill_color = color;
  layer_mark_dirty(&(path_layer->layer));
}

Layer* path_layer_get_layer(const PathLayer *path_layer) {
  return &((PathLayer *)path_layer)->layer;
}

