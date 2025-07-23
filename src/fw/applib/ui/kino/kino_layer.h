/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "kino_reel.h"
#include "kino_player.h"

#include "applib/ui/layer.h"

struct KinoLayer;
typedef struct KinoLayer KinoLayer;

typedef void (*KinoLayerDidStopCb)(KinoLayer *kino_layer, bool finished, void *context);

typedef struct {
  KinoLayerDidStopCb did_stop;
} KinoLayerCallbacks;

struct KinoLayer {
  Layer layer;
  KinoPlayer player;
  GColor background_color;
  GAlign alignment;
  KinoLayerCallbacks callbacks;
  void *context;
  bool invert_colors;
};

void kino_layer_init(KinoLayer *kino_layer, const GRect *frame);

void kino_layer_deinit(KinoLayer *kino_layer);

KinoLayer *kino_layer_create(GRect frame);

void kino_layer_destroy(KinoLayer *kino_layer);

Layer *kino_layer_get_layer(KinoLayer *kino_layer);

void kino_layer_set_reel(KinoLayer *kino_layer, KinoReel *reel, bool take_ownership);
void kino_layer_set_invert_colors(KinoLayer *kino_layer, bool invert);

//! @internal
void kino_layer_set_reel_with_resource(KinoLayer *kino_layer, uint32_t resource_id);
void kino_layer_set_reel_with_resource_system(KinoLayer *kino_layer, ResAppNum app_num,
                                              uint32_t resource_id, bool invert);

KinoReel *kino_layer_get_reel(KinoLayer *kino_layer);

KinoPlayer *kino_layer_get_player(KinoLayer *kino_layer);

GColor kino_layer_get_background_color(KinoLayer *kino_layer);

GAlign kino_layer_get_alignment(KinoLayer *kino_layer);

void kino_layer_set_alignment(KinoLayer *kino_layer, GAlign alignment);

void kino_layer_set_background_color(KinoLayer *kino_layer, GColor color);

void kino_layer_play(KinoLayer *kino_layer);

void kino_layer_play_section(KinoLayer *kino_layer, uint32_t from_position, uint32_t to_position);

ImmutableAnimation *kino_layer_create_play_animation(KinoLayer *kino_layer);

ImmutableAnimation *kino_layer_create_play_section_animation(
    KinoLayer *kino_layer, uint32_t from_position, uint32_t to_position);

void kino_layer_pause(KinoLayer *kino_layer);

void kino_layer_rewind(KinoLayer *kino_layer);

GRect kino_layer_get_reel_bounds(KinoLayer *kino_layer);

void kino_layer_set_callbacks(KinoLayer *kino_layer, KinoLayerCallbacks callbacks, void *context);
