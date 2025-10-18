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

//! This file generates a menu that lets the user select a quicklaunch app
//! The menu that is generated is the same as the "main menu" but with a
//! title

#include "settings_quick_launch_app_menu.h"
#include "settings_quick_launch_setup_menu.h"
#include "settings_quick_launch.h"
#include "settings_menu.h"
#include "settings_option_menu.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/window_stack.h"
#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_menu_data_source.h"
#include "resource/resource_ids.auto.h"
#include "shell/prefs.h"

typedef struct {
  AppMenuDataSource data_source;
  ButtonId button;
  int16_t selected;
} QuickLaunchAppMenuData;

#define NUM_CUSTOM_CELLS 1


/* Callback Functions */

static bool prv_app_filter_callback(struct AppMenuDataSource *source, AppInstallEntry *entry) {
  if (app_install_entry_is_watchface(entry)) {
    return false; // Skip watchfaces
  }
  if (app_install_entry_is_hidden(entry) &&
      !app_install_entry_is_quick_launch_visible_only(entry)) {
    return false; // Skip hidden apps unless they are quick launch visible
  }
  return true;
}

static uint16_t prv_menu_get_num_rows(OptionMenu *option_menu, void *context) {
  QuickLaunchAppMenuData *data = context;
  return app_menu_data_source_get_count(&data->data_source) + NUM_CUSTOM_CELLS;
}

static void prv_menu_draw_row(OptionMenu *option_menu, GContext* ctx, const Layer *cell_layer,
                              const GRect *text_frame, uint32_t row, bool selected, void *context) {

  QuickLaunchAppMenuData *data = context;
  const char *text = NULL;
  if (row == 0) {
    text = i18n_get("Disable", data);
  } else {
    AppMenuNode *node = app_menu_data_source_get_node_at_index(&data->data_source,
                                                               row - NUM_CUSTOM_CELLS);
    text = node->name;
  }
  option_menu_system_draw_row(option_menu, ctx, cell_layer, text_frame, text, selected, context);
}

static void prv_menu_select(OptionMenu *option_menu, int selection, void *context) {
  window_set_click_config_provider(&option_menu->window, NULL);

  QuickLaunchAppMenuData *data = context;
  if (selection == 0) {
    quick_launch_set_app(data->button, INSTALL_ID_INVALID);
    quick_launch_set_enabled(data->button, false);
    app_window_stack_pop(true);
  } else {
    AppMenuNode* app_menu_node =
        app_menu_data_source_get_node_at_index(&data->data_source, selection - NUM_CUSTOM_CELLS);
    quick_launch_set_app(data->button, app_menu_node->install_id);
    app_window_stack_pop(true);
  }
}

static void prv_menu_reload_data(void *context) {
  OptionMenu *option_menu = context;
  option_menu_reload_data(option_menu);
}

static void prv_menu_unload(OptionMenu *option_menu, void *context) {
  QuickLaunchAppMenuData *data = context;

  option_menu_destroy(option_menu);
  app_menu_data_source_deinit(&data->data_source);
  i18n_free_all(data);
  app_free(data);
}

void quick_launch_app_menu_window_push(ButtonId button) {
  QuickLaunchAppMenuData *data = app_zalloc_check(sizeof(*data));
  data->button = button;

  OptionMenu *option_menu = option_menu_create();

  app_menu_data_source_init(&data->data_source, &(AppMenuDataSourceCallbacks) {
    .changed = prv_menu_reload_data,
    .filter = prv_app_filter_callback,
  }, option_menu);

  const AppInstallId install_id = quick_launch_get_app(button);
  const int app_index = app_menu_data_source_get_index_of_app_with_install_id(&data->data_source,
                                                                              install_id);

  const OptionMenuConfig config = {
    .title = i18n_get(i18n_noop("Quick Launch"), data),
    .choice = (install_id == INSTALL_ID_INVALID) ? 0 : (app_index + NUM_CUSTOM_CELLS),
    .status_colors = { GColorWhite, GColorBlack, },
    .highlight_colors = { shell_prefs_get_settings_menu_highlight_color(), GColorWhite },
    .icons_enabled = true,
  };
  option_menu_configure(option_menu, &config);
  option_menu_set_callbacks(option_menu, &(OptionMenuCallbacks) {
    .select = prv_menu_select,
    .get_num_rows = prv_menu_get_num_rows,
    .draw_row = prv_menu_draw_row,
    .unload = prv_menu_unload,
  }, data);

  const bool animated = true;
  app_window_stack_push(&option_menu->window, animated);
}
