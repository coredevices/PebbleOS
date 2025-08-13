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

#include "launcher_app_glance_service.h"

#include "process_management/app_menu_data_source.h"

#if PLATFORM_ROBERT || PLATFORM_OBELIX
#define LAUNCHER_MENU_LAYER_TITLE_FONT (FONT_KEY_GOTHIC_24_BOLD)
#define LAUNCHER_MENU_LAYER_SUBTITLE_FONT (FONT_KEY_GOTHIC_18)
#else
#define LAUNCHER_MENU_LAYER_TITLE_FONT (FONT_KEY_GOTHIC_18_BOLD)
#define LAUNCHER_MENU_LAYER_SUBTITLE_FONT (FONT_KEY_GOTHIC_14)
#endif

#define LAUNCHER_MENU_LAYER_SELECTION_BACKGROUND_COLOR (PBL_IF_COLOR_ELSE(GColorVividCerulean, \
                                                                          GColorBlack))

typedef struct LauncherMenuLayer {
  Layer container_layer;
  MenuLayer menu_layer;
#if PBL_ROUND
  Layer up_arrow_layer;
  Layer down_arrow_layer;
#endif
  GFont title_font;
  GFont subtitle_font;
  AppMenuDataSource *data_source;
  LauncherAppGlanceService glance_service;
  bool selection_animations_enabled;
  AppInstallId app_to_launch_after_next_render;
} LauncherMenuLayer;

typedef struct LauncherMenuLayerSelectionState {
  int16_t scroll_offset_y;
  uint16_t row_index;
} LauncherMenuLayerSelectionState;

void launcher_menu_layer_init(LauncherMenuLayer *launcher_menu_layer,
                              AppMenuDataSource *data_source);

Layer *launcher_menu_layer_get_layer(LauncherMenuLayer *launcher_menu_layer);

void launcher_menu_layer_set_click_config_onto_window(LauncherMenuLayer *launcher_menu_layer,
                                                      Window *window);

void launcher_menu_layer_reload_data(LauncherMenuLayer *launcher_menu_layer);

void launcher_menu_layer_set_selection_state(LauncherMenuLayer *launcher_menu_layer,
                                             const LauncherMenuLayerSelectionState *new_state);

void launcher_menu_layer_get_selection_state(const LauncherMenuLayer *launcher_menu_layer,
                                             LauncherMenuLayerSelectionState *state_out);

void launcher_menu_layer_get_selection_vertical_range(const LauncherMenuLayer *launcher_menu_layer,
                                                      GRangeVertical *vertical_range_out);

void launcher_menu_layer_set_selection_animations_enabled(LauncherMenuLayer *launcher_menu_layer,
                                                          bool enabled);

void launcher_menu_layer_deinit(LauncherMenuLayer *launcher_menu_layer);
