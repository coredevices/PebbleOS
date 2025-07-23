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

#include "applib/ui/animation.h"

struct KinoPlayer;
typedef struct KinoPlayer KinoPlayer;

typedef void (*KinoPlayerFrameDidChangeCb)(KinoPlayer *player, void *context);
typedef void (*KinoPlayerDidStopCb)(KinoPlayer *player, bool finished, void *context);

typedef struct {
  KinoPlayerFrameDidChangeCb frame_did_change;
  KinoPlayerDidStopCb did_stop;
} KinoPlayerCallbacks;

struct KinoPlayer {
  KinoReel *reel;
  bool owns_reel;
  Animation *animation;
  KinoPlayerCallbacks callbacks;
  uint32_t from_elapsed_ms;
  uint32_t to_elapsed_ms;
  void *context;
};

void kino_player_set_callbacks(KinoPlayer *player, KinoPlayerCallbacks callbacks, void *context);

void kino_player_set_reel(KinoPlayer *player, KinoReel *reel, bool take_ownership);

//! @internal
void kino_player_set_reel_with_resource(KinoPlayer *player, uint32_t resource_id);
void kino_player_set_reel_with_resource_system(KinoPlayer *player, ResAppNum app_num,
                                               uint32_t resource_id);

KinoReel *kino_player_get_reel(KinoPlayer *player);

void kino_player_play(KinoPlayer *player);

void kino_player_play_section(KinoPlayer *player, uint32_t from_elapsed_ms, uint32_t to_elapsed_ms);

//! @internal
//! Creates a play animation that can be composed with complex animations. This animation will call
//! the KinoPlayer callbacks when it animates just as directly playing the KinoPlayer would.
//! Creating another play animation or directly playing, pausing or rewinding the player will
//! immediately unschedule the returned animation, even if it has not been scheduled yet.
//! /note The returned animation is an immutable animation and thus does not have the full range of
//! animation setters available for use. It is as though it has already been scheduled.
//! @param player KinoPlayer to create a play animation of
//! @return a pointer to a ImmutableAnimation object that plays the KinoPlayer when scheduled
ImmutableAnimation *kino_player_create_play_animation(KinoPlayer *player);

ImmutableAnimation *kino_player_create_play_section_animation(
    KinoPlayer *player, uint32_t from_elapsed_ms, uint32_t to_elapsed_ms);

void kino_player_pause(KinoPlayer *player);

void kino_player_rewind(KinoPlayer *player);

void kino_player_deinit(KinoPlayer *player);

void kino_player_draw(KinoPlayer *player, GContext *ctx, GPoint offset);

void kino_player_draw_processed(KinoPlayer *player, GContext *ctx, GPoint offset,
                                KinoReelProcessor *processor);