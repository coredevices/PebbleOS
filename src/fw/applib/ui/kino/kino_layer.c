/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kino_layer.h"

#include "applib/applib_malloc.auto.h"
#include "applib/graphics/graphics.h"

static void prv_invert_pdc_colors(GDrawCommandProcessor *processor, GDrawCommand *processed_command,
                                  size_t processed_command_max_size, const GDrawCommandList *list,
                                  const GDrawCommand *command) {
  gdraw_command_set_stroke_color(
      processed_command,
      gcolor_invert(gdraw_command_get_stroke_color((GDrawCommand *)command)));
  gdraw_command_set_fill_color(
      processed_command,
      gcolor_invert(gdraw_command_get_fill_color((GDrawCommand *)command)));
}

GDrawCommandProcessor prv_gdraw_inv_processor = {
  .command = prv_invert_pdc_colors,
};

KinoReelProcessor PRV_INVERT_COLORS_PROCESSOR = {.draw_command_processor = &prv_gdraw_inv_processor};

static void prv_update_proc(Layer *layer, GContext *ctx) {
  KinoLayer *kino_layer = (KinoLayer *)layer;

  // Fill background
  if (kino_layer->background_color.a != 0) {
    graphics_context_set_fill_color(ctx, kino_layer->background_color);
    graphics_fill_rect(ctx, &kino_layer->layer.bounds);
  }

  // Draw Reel
  KinoReel *reel = kino_player_get_reel(&kino_layer->player);
  if (reel == NULL) {
    return;
  }

  const GRect reel_bounds = kino_layer_get_reel_bounds(kino_layer);

  KinoReelProcessor processor = kino_layer->invert_colors ? PRV_INVERT_COLORS_PROCESSOR : (KinoReelProcessor){};
  kino_player_draw_processed(&kino_layer->player, ctx, reel_bounds.origin, &processor);
}

//////////////////////
// Player Callbacks
//////////////////////

static void prv_player_frame_did_change(KinoPlayer *player, void *context) {
  KinoLayer *kino_layer = context;
  layer_mark_dirty((Layer *)kino_layer);
}

static void prv_player_did_stop(KinoPlayer *player, bool finished, void *context) {
  KinoLayer *kino_layer = context;
  if (kino_layer->callbacks.did_stop) {
    kino_layer->callbacks.did_stop(kino_layer, finished, kino_layer->context);
  }
}

///////////////////////////////////////////
// Kino Layer API
///////////////////////////////////////////

void kino_layer_init(KinoLayer *kino_layer, const GRect *frame) {
  *kino_layer = (KinoLayer){};
  // init layer
  layer_init(&kino_layer->layer, frame);
  layer_set_update_proc(&kino_layer->layer, prv_update_proc);
  // init kino layer
  kino_layer->background_color = GColorClear;
  // init kino player
  kino_player_set_callbacks(&kino_layer->player, (KinoPlayerCallbacks){
    .frame_did_change = prv_player_frame_did_change,
    .did_stop = prv_player_did_stop,
  }, kino_layer);
}

void kino_layer_deinit(KinoLayer *kino_layer) {
  kino_player_deinit(&kino_layer->player);
  layer_deinit(&kino_layer->layer);
}

KinoLayer *kino_layer_create(GRect frame) {
  KinoLayer *layer = applib_type_malloc(KinoLayer);
  if (layer) {
    kino_layer_init(layer, &frame);
  }

  return layer;
}

void kino_layer_destroy(KinoLayer *kino_layer) {
  if (kino_layer == NULL) {
    return;
  }

  kino_layer_deinit(kino_layer);
  applib_free(kino_layer);
}

Layer *kino_layer_get_layer(KinoLayer *kino_layer) {
  if (kino_layer) {
    return &kino_layer->layer;
  } else {
    return NULL;
  }
}

void kino_layer_set_reel(KinoLayer *kino_layer, KinoReel *reel, bool take_ownership) {
  kino_player_set_reel(&kino_layer->player, reel, take_ownership);
}

void kino_layer_set_invert_colors(KinoLayer *kino_layer, bool invert) {
  // Store the invert flag in the LSB of the context pointer
  kino_layer->invert_colors = invert;
}

void kino_layer_set_reel_with_resource(KinoLayer *kino_layer, uint32_t resource_id) {
  kino_player_set_reel_with_resource(&kino_layer->player, resource_id);
}

void kino_layer_set_reel_with_resource_system(KinoLayer *kino_layer, ResAppNum app_num,
                                              uint32_t resource_id, bool invert) {
  kino_layer_set_invert_colors(kino_layer, invert);
  kino_player_set_reel_with_resource_system(&kino_layer->player, app_num, resource_id);
}

KinoReel *kino_layer_get_reel(KinoLayer *kino_layer) {
  return kino_player_get_reel(&kino_layer->player);
}

KinoPlayer *kino_layer_get_player(KinoLayer *kino_layer) {
  return &kino_layer->player;
}

void kino_layer_set_alignment(KinoLayer *kino_layer, GAlign alignment) {
  kino_layer->alignment = alignment;
  layer_mark_dirty(&kino_layer->layer);
}

void kino_layer_set_background_color(KinoLayer *kino_layer, GColor color) {
  kino_layer->background_color = color;
  layer_mark_dirty(&kino_layer->layer);
}

void kino_layer_play(KinoLayer *kino_layer) {
  kino_player_play(&kino_layer->player);
}

void kino_layer_play_section(KinoLayer *kino_layer, uint32_t from_position, uint32_t to_position) {
  kino_player_play_section(&kino_layer->player, from_position, to_position);
}

ImmutableAnimation *kino_layer_create_play_animation(KinoLayer *kino_layer) {
  return kino_player_create_play_animation(&kino_layer->player);
}

ImmutableAnimation *kino_layer_create_play_section_animation(
    KinoLayer *kino_layer, uint32_t from_position, uint32_t to_position) {
  return kino_player_create_play_section_animation(&kino_layer->player, from_position,
                                                   to_position);
}

void kino_layer_pause(KinoLayer *kino_layer) {
  kino_player_pause(&kino_layer->player);
}

void kino_layer_rewind(KinoLayer *kino_layer) {
  kino_player_rewind(&kino_layer->player);
}

GColor kino_layer_get_background_color(KinoLayer *kino_layer) {
  return kino_layer->background_color;
}

GAlign kino_layer_get_alignment(KinoLayer *kino_layer) {
  return kino_layer->alignment;
}

GRect kino_layer_get_reel_bounds(KinoLayer *kino_layer) {
  KinoPlayer *player = kino_layer_get_player(kino_layer);
  KinoReel *reel = player ? kino_player_get_reel(player) : NULL;
  if (!reel) {
    return GRectZero;
  }

  const GSize size = kino_reel_get_size(reel);
  GRect rect = (GRect){{0, 0}, size};
  grect_align(&rect, &kino_layer->layer.bounds, kino_layer->alignment, false /*clips*/);
  return rect;
}

void kino_layer_set_callbacks(KinoLayer *kino_layer, KinoLayerCallbacks callbacks, void *context) {
  kino_layer->callbacks = callbacks;
  kino_layer->context = context;
}
