/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/kino/kino_layer.h>

PBL_WEAK void kino_layer_init(KinoLayer *kino_layer, const GRect *frame) {
}

PBL_WEAK void kino_layer_deinit(KinoLayer *kino_layer) {
}

PBL_WEAK KinoLayer *kino_layer_create(GRect frame) {
  return nullptr;
}

PBL_WEAK void kino_layer_destroy(KinoLayer *kino_layer) {
}

PBL_WEAK Layer *kino_layer_get_layer(KinoLayer *kino_layer) {
  return nullptr;
}

PBL_WEAK void kino_layer_set_reel(KinoLayer *kino_layer, KinoReel *reel, bool take_ownership) {
}

PBL_WEAK void kino_layer_set_reel_with_resource(KinoLayer *kino_layer, uint32_t resource_id) {
}

PBL_WEAK void kino_layer_set_reel_with_resource_system(KinoLayer *kino_layer, ResAppNum app_num,
                                                       uint32_t resource_id, bool invert) {
}

PBL_WEAK KinoReel *kino_layer_get_reel(KinoLayer *kino_layer) {
  return nullptr;
}

PBL_WEAK KinoPlayer *kino_layer_get_player(KinoLayer *kino_layer) {
  return nullptr;
}

PBL_WEAK GColor kino_layer_get_background_color(KinoLayer *kino_layer) {
  return GColorClear;
}

PBL_WEAK GAlign kino_layer_get_alignment(KinoLayer *kino_layer) {
  return 0;
}

PBL_WEAK void kino_layer_set_alignment(KinoLayer *kino_layer, GAlign alignment) {
}

PBL_WEAK void kino_layer_set_background_color(KinoLayer *kino_layer, GColor color) {
}

PBL_WEAK void kino_layer_play(KinoLayer *kino_layer) {
}

PBL_WEAK void kino_layer_play_section(KinoLayer *kino_layer, uint32_t from_position,
                                      uint32_t to_position) {
}

PBL_WEAK void kino_layer_pause(KinoLayer *kino_layer) {
}

PBL_WEAK void kino_layer_rewind(KinoLayer *kino_layer) {
}

PBL_WEAK GRect kino_layer_get_reel_bounds(KinoLayer *kino_layer) {
  return (GRect){};
}

PBL_WEAK void kino_layer_set_callbacks(KinoLayer *kino_layer, KinoLayerCallbacks callbacks,
                                       void *context) {
}
