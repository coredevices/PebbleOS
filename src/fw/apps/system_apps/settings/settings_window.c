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
#include "settings_activity_tracker.h"
#include "settings_system.h"
#include "settings_bluetooth.h"
#include "settings_display.h"
#include "settings_menu.h"
#include "settings_notifications.h"
#include "settings_quick_launch.h"
#include "settings_remote.h"
#include "settings_time.h"
#include "settings_window.h"

#include "applib/app.h"
#include "applib/battery_state_service.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/fw_reset.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "services/common/system_task.h"
#include "system/bootbits.h"
#include "system/passert.h"
#include "shell/prefs.h"

#include <stdio.h>
#include <string.h>

typedef struct SettingsData {
  Window window;
  StatusBarLayer status_layer;
  MenuLayer menu_layer;

  SettingsMenuItem current_category; //!< SettingsMenuItem_Invalid if not currently in a category.

  const char *title;
  SettingsCallbacks *callbacks;

  ClickConfigProvider menu_layer_click_config; //! HACK: Used to register a back click.
} SettingsData;


// Filter category helpers
//////////////////////////

static SettingsCallbacks *prv_get_current_callbacks(SettingsData *data) {
  PBL_ASSERTN(data && data->callbacks);
  return data->callbacks;
}

// Menu appearance helpers
//////////////////////////

static void prv_set_sub_menu_colors(GContext *ctx, const Layer *cell_layer, bool highlight) {
  if (highlight) {
    graphics_context_set_fill_color(ctx, shell_prefs_get_settings_menu_highlight_color());
    graphics_context_set_text_color(ctx, GColorWhite);
  } else {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_text_color(ctx, GColorBlack);
  }
  graphics_fill_rect(ctx, &cell_layer->bounds);
}

// Menu Layer Handling
//////////////////////

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  SettingsData *data = context;

  const uint16_t row = cell_index->row;
  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->select_click) {
    callbacks->select_click(callbacks, row);
  }
}

static void prv_selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index,
                                           MenuIndex old_index, void *context) {
  SettingsData *data = context;

  const uint16_t new_row = new_index.row;
  const uint16_t old_row = old_index.row;

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->selection_changed) {
    callbacks->selection_changed(callbacks, new_row, old_row);
  }
}

static void prv_selection_will_change_callback(MenuLayer *menu_layer, MenuIndex *new_index,
                                               MenuIndex old_index, void *context) {
  SettingsData *data = context;

  const uint16_t old_row = old_index.row;

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->selection_will_change) {
    callbacks->selection_will_change(callbacks, &new_index->row, old_row);
  }
}

static void prv_draw_row_callback(GContext *ctx, const Layer *cell_layer,
                                  MenuIndex *cell_index, void *context) {
  SettingsData *data = context;

  uint16_t row = cell_index->row;
  const uint16_t section = cell_index->section;
  PBL_ASSERTN(section < SettingsMenuItem_Count);

  bool highlight = menu_cell_layer_is_highlighted(cell_layer);

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  prv_set_sub_menu_colors(ctx, cell_layer, highlight);
  if (callbacks->draw_row) {
    const bool is_selected = menu_cell_layer_is_highlighted(cell_layer);
    callbacks->draw_row(callbacks, ctx, cell_layer, row, is_selected);
  }
}

static uint16_t prv_get_num_rows_callback(MenuLayer *menu_layer,
                                          uint16_t section_index, void *context) {
  PBL_ASSERTN(section_index < SettingsMenuItem_Count);
  SettingsData *data = context;

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  return callbacks->num_rows ? callbacks->num_rows(callbacks) : (uint16_t)0;
}

static int16_t prv_get_cell_height_callback(MenuLayer *menu_layer,
                                            MenuIndex *cell_index, void *context) {
  PBL_ASSERTN(cell_index->section < SettingsMenuItem_Count);
  SettingsData *data = context;

  const uint16_t row = cell_index->row;
  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  const bool is_selected = menu_layer_is_index_selected(menu_layer, cell_index);
  return (callbacks->row_height) ?
      callbacks->row_height(callbacks, row, is_selected) :
      PBL_IF_RECT_ELSE(menu_cell_basic_cell_height(),
                       (is_selected ? MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT :
                                      MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT));
}

// Settings Window:
////////////////////////

static void prv_settings_window_load(Window *window) {
  SettingsData *data = window_get_user_data(window);

  StatusBarLayer *status_layer = &data->status_layer;
  status_bar_layer_init(status_layer);
  const char *title = settings_menu_get_status_name(data->current_category);
  status_bar_layer_set_title(status_layer, i18n_get(title, data), false, false);
  status_bar_layer_set_colors(status_layer, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(status_layer, OPTION_MENU_STATUS_SEPARATOR_MODE);
  layer_add_child(&data->window.layer, status_bar_layer_get_layer(status_layer));

  GRect bounds = grect_inset(data->window.layer.bounds, (GEdgeInsets) {
    .top = STATUS_BAR_LAYER_HEIGHT,
    .bottom = PBL_IF_RECT_ELSE(0, STATUS_BAR_LAYER_HEIGHT),
  });

  // Create the menu
  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &bounds);
  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows_callback,
    .get_cell_height = prv_get_cell_height_callback,
    .draw_row = prv_draw_row_callback,
    .select_click = prv_select_callback,
    .selection_changed = prv_selection_changed_callback,
    .selection_will_change = prv_selection_will_change_callback,
  });
  menu_layer_set_normal_colors(menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(menu_layer, shell_prefs_get_settings_menu_highlight_color(), GColorWhite);
  menu_layer_set_click_config_onto_window(menu_layer, &data->window);
  layer_add_child(&data->window.layer, menu_layer_get_layer(menu_layer));

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->get_initial_selection) {
    const uint16_t selected_row = callbacks->get_initial_selection(data->callbacks);
    menu_layer_set_selected_index(menu_layer, MenuIndex(0, selected_row), MenuRowAlignCenter,
                                  false /* animated */);
  }

  if (callbacks->expand) {
    callbacks->expand(data->callbacks);
  }
}

static void prv_settings_window_appear(Window *window) {
  SettingsData *data = window_get_user_data(window);
  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->appear) {
    callbacks->appear(data->callbacks);
  }
}

static void prv_settings_window_unload(Window *window) {
  SettingsData *data = window_get_user_data(window);
  // Call the hide callback for the currently open category.
  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->hide) {
    callbacks->hide(callbacks);
  }
  i18n_free_all(data);
  menu_layer_deinit(&data->menu_layer);
  status_bar_layer_deinit(&data->status_layer);
  settings_window_destroy(window);
}

Window *settings_window_create(SettingsMenuItem category, SettingsCallbacks *callbacks) {
  PBL_ASSERTN(callbacks && (category < SettingsMenuItem_Count));

  SettingsData *data = app_zalloc_check(sizeof(*data));

  data->current_category = category;
  data->title = settings_menu_get_submodule_info(category)->name;
  data->callbacks = callbacks;

  app_state_set_user_data(data);

  window_init(&data->window, WINDOW_NAME("Settings Window"));
  window_set_user_data(&data->window, data);
  window_set_window_handlers(&data->window, &(WindowHandlers){
    .load = prv_settings_window_load,
    .appear = prv_settings_window_appear,
    .unload = prv_settings_window_unload,
  });

  return &data->window;
}

void settings_window_destroy(Window *window) {
  SettingsData *data = window_get_user_data(window);

  SettingsCallbacks *callbacks = prv_get_current_callbacks(data);
  if (callbacks->deinit) {
    callbacks->deinit(data->callbacks);
  }

  i18n_free_all(data);
  app_free(data);
}

void settings_menu_mark_dirty(SettingsMenuItem category) {
  SettingsData *data = app_state_get_user_data();
  if (data->current_category == category) {
    layer_mark_dirty(menu_layer_get_layer(&data->menu_layer));
  }
}

void settings_menu_reload_data(SettingsMenuItem category) {
  SettingsData *data = app_state_get_user_data();
  if (data->current_category == category) {
    menu_layer_reload_data(&data->menu_layer);
  }
}

int16_t settings_menu_get_selected_row(SettingsMenuItem category) {
  SettingsData *data = app_state_get_user_data();
  if (data->current_category == category) {
    return menu_layer_get_selected_index(&data->menu_layer).row;
  }
  return 0; // say first row is selected if all else fails
}
