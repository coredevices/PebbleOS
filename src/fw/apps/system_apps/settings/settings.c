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

#include "settings.h"
#include "settings_menu.h"
#include "settings_window.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "shell/prefs.h"

#define SETTINGS_CATEGORY_MENU_CELL_UNFOCUSED_ROUND_VERTICAL_PADDING 14

typedef struct {
  Window window;
  MenuLayer menu_layer;
} SettingsAppData;

static uint16_t prv_get_num_rows_callback(MenuLayer *menu_layer,
                                          uint16_t section_index, void *context) {
  return SettingsMenuItem_Count;
}

static void prv_draw_row_callback(GContext *ctx, const Layer *cell_layer,
                                  MenuIndex *cell_index, void *context) {
  SettingsAppData *data = context;

  PBL_ASSERTN(cell_index->row < SettingsMenuItem_Count);

  const char *category_title = settings_menu_get_submodule_info(cell_index->row)->name;
  const char *title = i18n_get(category_title, data);
  menu_layer_set_highlight_colors(&(data->menu_layer),
                                shell_prefs_get_settings_menu_highlight_color(),
                                GColorWhite);
  menu_cell_basic_draw(ctx, cell_layer, title, NULL, NULL);
}

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  settings_menu_push(cell_index->row);
}

static int16_t prv_get_cell_height_callback(MenuLayer *menu_layer,
                                            MenuIndex *cell_index, void *context) {
  PBL_ASSERTN(cell_index->row < SettingsMenuItem_Count);

#if PBL_RECT
  const int16_t category_title_height = 37;
  return category_title_height;
#else
  const int16_t focused_cell_height = MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT;
  const int16_t unfocused_cell_height =
      ((DISP_ROWS - focused_cell_height) / 2) -
          SETTINGS_CATEGORY_MENU_CELL_UNFOCUSED_ROUND_VERTICAL_PADDING;
  return menu_layer_is_index_selected(menu_layer, cell_index) ? focused_cell_height :
                                                                unfocused_cell_height;
#endif
}

static int16_t prv_get_separator_height_callback(MenuLayer *menu_layer,
                                                 MenuIndex *cell_index,
                                                 void *context) {
  return 0;
}

static void prv_window_load(Window *window) {
  SettingsAppData *data = window_get_user_data(window);

  // Create the menu
  GRect bounds = data->window.layer.bounds;
#if PBL_ROUND
  bounds = grect_inset_internal(bounds, 0,
                                SETTINGS_CATEGORY_MENU_CELL_UNFOCUSED_ROUND_VERTICAL_PADDING);
#endif
  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &bounds);
  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows_callback,
    .get_cell_height = prv_get_cell_height_callback,
    .draw_row = prv_draw_row_callback,
    .select_click = prv_select_callback,
    .get_separator_height = prv_get_separator_height_callback
  });
  menu_layer_set_normal_colors(menu_layer,
                               PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite),
                               PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  menu_layer_set_highlight_colors(menu_layer,
                                  shell_prefs_get_settings_menu_highlight_color(),
                                  GColorWhite);
  menu_layer_set_click_config_onto_window(menu_layer, &data->window);

  layer_add_child(&data->window.layer, menu_layer_get_layer(menu_layer));
}

static void prv_window_unload(Window *window) {
  SettingsAppData *data = window_get_user_data(window);
  menu_layer_deinit(&data->menu_layer);
  app_free(data);
}

static void handle_init(void) {
  SettingsAppData *data = app_zalloc_check(sizeof(SettingsAppData));

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Settings"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_set_background_color(window, GColorBlack);
  app_window_stack_push(window, true);
}

static void handle_deinit(void) {
  // Window unload deinits everything
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd *settings_get_app_info() {
  static const PebbleProcessMdSystem s_settings_app = {
    .common = {
      .main_func = s_main,
      // UUID: 07e0d9cb-8957-4bf7-9d42-35bf47caadfe
      .uuid = {0x07, 0xe0, 0xd9, 0xcb, 0x89, 0x57, 0x4b, 0xf7,
               0x9d, 0x42, 0x35, 0xbf, 0x47, 0xca, 0xad, 0xfe},
    },
    .name = i18n_noop("Settings"),
#if CAPABILITY_HAS_APP_GLANCES
    .icon_resource_id = RESOURCE_ID_SETTINGS_TINY,
#elif PLATFORM_TINTIN
    .icon_resource_id = RESOURCE_ID_MENU_LAYER_SETTINGS_APP_ICON,
#endif
  };
  return (const PebbleProcessMd*) &s_settings_app;
}
