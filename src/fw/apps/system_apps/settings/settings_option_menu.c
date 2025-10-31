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

#include "settings_option_menu.h"

#include "settings_menu.h"

#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"

static void prv_menu_unload(OptionMenu *option_menu, void *context) {
  SettingsOptionMenuData *data = context;
  if (data->callbacks.unload) {
    data->callbacks.unload(option_menu, data);
  }
  option_menu_destroy(option_menu);
  i18n_free_all(option_menu);
  task_free(context);
}

static uint16_t prv_menu_get_num_rows(OptionMenu *option_menu, void *context) {
  return ((SettingsOptionMenuData *)context)->num_rows;
}

static void prv_menu_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                              const GRect *cell_frame, uint32_t row, bool selected, void *context) {
  SettingsOptionMenuData *data = context;
  const char *title = i18n_get(data->rows[row], option_menu);
  option_menu_system_draw_row(option_menu, ctx, cell_layer, cell_frame, title, selected, context);
}

OptionMenu *settings_option_menu_create(
    const char *i18n_title_key, OptionMenuContentType content_type, int choice,
    const OptionMenuCallbacks *callbacks_ref, uint16_t num_rows, bool icons_enabled,
    const char **rows, void *context) {
  OptionMenu *option_menu = option_menu_create();
  if (!option_menu) {
    return NULL;
  }
  const OptionMenuConfig config = {
    .title = i18n_get(i18n_title_key, option_menu),
    .content_type = content_type,
    .choice = choice,
    .status_colors = { GColorWhite, GColorBlack },
    .highlight_colors = { shell_prefs_get_settings_menu_highlight_color(), GColorWhite },
    .icons_enabled = icons_enabled,
  };
  option_menu_configure(option_menu, &config);
  SettingsOptionMenuData *data = task_malloc_check(sizeof(SettingsOptionMenuData));
  OptionMenuCallbacks callbacks = *callbacks_ref;
  *data = (SettingsOptionMenuData) {
    .callbacks = callbacks,
    .context = context,
    .num_rows = num_rows,
    .rows = rows,
  };
  callbacks.draw_row = prv_menu_draw_row;
  callbacks.get_num_rows = prv_menu_get_num_rows;
  callbacks.unload = prv_menu_unload;
  option_menu_set_callbacks(option_menu, &callbacks, data);
  return option_menu;
}

OptionMenu *settings_option_menu_push(
    const char *i18n_title_key, OptionMenuContentType content_type, int choice,
    const OptionMenuCallbacks *callbacks_ref, uint16_t num_rows, bool icons_enabled,
    const char **rows, void *context) {
  OptionMenu * const option_menu = settings_option_menu_create(
      i18n_title_key, content_type, choice, callbacks_ref, num_rows, icons_enabled, rows, context);
  if (option_menu) {
    const bool animated = true;
    app_window_stack_push(&option_menu->window, animated);
  }
  return option_menu;
}

void *settings_option_menu_get_context(SettingsOptionMenuData *data) {
  return data->context;
}
