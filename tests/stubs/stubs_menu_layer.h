/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/menu_layer.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title,
                                   const char *subtitle, GBitmap *icon) {
}

void PBL_WEAK menu_cell_title_draw(GContext *ctx, const Layer *cell_layer, const char *title) {
}

void PBL_WEAK menu_cell_basic_header_draw(GContext *ctx, const Layer *cell_layer,
                                          const char *title) {
}

void PBL_WEAK menu_layer_init(MenuLayer *menu_layer, const GRect *frame) {
}

MenuLayer *PBL_WEAK menu_layer_create(GRect frame) {
  return NULL;
}

void PBL_WEAK menu_layer_deinit(MenuLayer *menu_layer) {
}

void PBL_WEAK menu_layer_destroy(MenuLayer *menu_layer) {
}

Layer *PBL_WEAK menu_layer_get_layer(const MenuLayer *menu_layer) {
  return NULL;
}

ScrollLayer *PBL_WEAK menu_layer_get_scroll_layer(const MenuLayer *menu_layer) {
  return NULL;
}

void PBL_WEAK menu_layer_set_callbacks(MenuLayer *menu_layer, void *callback_context,
                                       const MenuLayerCallbacks *callbacks) {
}

void PBL_WEAK menu_layer_set_callbacks__deprecated(MenuLayer *menu_layer, void *callback_context,
                                                   const MenuLayerCallbacks *callbacks) {
}

void PBL_WEAK menu_layer_set_click_config_onto_window(MenuLayer *menu_layer,
                                                      struct Window *window) {
}

void PBL_WEAK menu_layer_set_selected_next(MenuLayer *menu_layer, bool up,
                                           MenuRowAlign scroll_align, bool animated) {
}

void PBL_WEAK menu_layer_set_selected_index(MenuLayer *menu_layer, MenuIndex index,
                                            MenuRowAlign scroll_align, bool animated) {
}

void PBL_WEAK menu_layer_reload_data(MenuLayer *menu_layer) {
}
