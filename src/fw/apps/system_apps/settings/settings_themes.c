/*
 * Copyright 2025 Elad Dvash
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


#include "settings_themes.h"
#include "settings_menu.h"
#include "settings_option_menu.h"
#include "settings_window.h"

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/menu_layer.h"
#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "util/size.h"

/* Per-window data for this settings module. */
typedef struct SettingsThemesData {
  SettingsCallbacks callbacks;
} SettingsThemesData;


/* Menu row indices for the Themes menu. */
typedef enum ThemesMenuIndex {
  ThemesMenuIndex_Apps,
  ThemesMenuIndex_Settings,

  ThemesMenuIndexCount
} ThemesMenuIndex;

/* Free i18n strings and allocated context when the menu is torn down. */
static void prv_deinit_cb(SettingsCallbacks *context) {
  i18n_free_all(context);
  app_free(context);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return ThemesMenuIndexCount;
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsThemesData *data = (SettingsThemesData *)context;
  const char *title = NULL;
  const char *subtitle = NULL;

  switch ((ThemesMenuIndex)row) {
    case ThemesMenuIndex_Apps:
      /* Title for the Apps accent color item. */
      title = i18n_noop("Apps Accent");
      break;
    case ThemesMenuIndex_Settings:
      /* Title for the Settings accent color item. */
      title = i18n_noop("Settings Accent");
      break;
    case ThemesMenuIndexCount:
      break;
  }

  PBL_ASSERTN(title);
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static const char* color_names_short[ARRAY_LENGTH(s_color_definitions_short)];
static bool color_names_short_initialized = false;
static const char* color_names[ARRAY_LENGTH(s_color_definitions)];
static bool color_names_initialized = false;

static const char** prv_get_color_names(bool short_list) {
  if (short_list) {
    if (!color_names_short_initialized) {
      for (size_t i = 0; i < ARRAY_LENGTH(s_color_definitions_short); i++) {
        color_names_short[i] = (char*)s_color_definitions_short[i].name;
      }
      color_names_short_initialized = true;
    }
    return color_names_short;
  }
  if (!color_names_initialized) {
    for (size_t i = 0; i < ARRAY_LENGTH(s_color_definitions); i++) {
      color_names[i] = (char*)s_color_definitions[i].name;
    }
    color_names_initialized = true;
  }
  return color_names;
}




static int prv_color_to_index(GColor color, bool short_list, GColor default_color) {
  if (color.argb == GColorClear.argb || color.argb == default_color.argb) {
    return 0;
  }
  if (short_list) {
    for (size_t i = 0; i < ARRAY_LENGTH(s_color_definitions_short); i++) {
      if ((uint8_t)(color.argb) == (uint8_t)(s_color_definitions_short[i].value.argb)) {
        return i;
      }
    }
    return -1;
  }
  for (size_t i = 0; i < ARRAY_LENGTH(s_color_definitions); i++) {
    if ((uint8_t)(color.argb) == (uint8_t)(s_color_definitions[i].value.argb)) {
      return i;
    }
  }
  return -1;
}


/////////////////////////////
// Apps Accent Color Settings
/////////////////////////////

static void prv_apps_color_menu_select(OptionMenu *option_menu, int selection, void *context) {
  if (selection == 0){
    /* Default option selected -> restore default color. */
    shell_prefs_set_apps_menu_highlight_color(DEFAULT_APPS_HIGHLIGHT_COLOR);
  }
  else{
    shell_prefs_set_apps_menu_highlight_color(s_color_definitions[selection].value);
  }
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_option_apps_menu_selection_will_change(OptionMenu *option_menu,
                                             uint16_t new_row,
                                             uint16_t old_row,
                                             void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color = s_color_definitions[new_row].value;
  if (color.argb != GColorClear.argb) {
    option_menu_set_highlight_colors(option_menu, color, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
  else {
    option_menu_set_highlight_colors(option_menu, DEFAULT_APPS_HIGHLIGHT_COLOR, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
}

static void prv_push_apps_color_menu(SettingsThemesData *data) {
  const char *title = i18n_noop("Apps Menu Accent");
  int selected = prv_color_to_index(shell_prefs_get_apps_menu_highlight_color(), false, DEFAULT_APPS_HIGHLIGHT_COLOR);
  const char** color_names = prv_get_color_names(false);
  const OptionMenuCallbacks callbacks = {
    .select = prv_apps_color_menu_select,
    .selection_will_change = prv_option_apps_menu_selection_will_change,
  };
  if (selected < 0) {
    WTF;
  }
  OptionMenu * const option_menu = settings_option_menu_create(
      title, OptionMenuContentType_SingleLine, selected, &callbacks,
      ARRAY_LENGTH(s_color_definitions), true /* icons_enabled */, color_names, data);

  if (option_menu) {
    const bool animated = true;
    if (selected == 0) {
      option_menu_set_highlight_colors(option_menu, DEFAULT_APPS_HIGHLIGHT_COLOR, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    }
    else {
      option_menu_set_highlight_colors(option_menu, s_color_definitions[selected].value, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    }
    app_window_stack_push(&option_menu->window, animated);
  }
}


/////////////////////////////
// Short Apps Accent Color Settings
/////////////////////////////

static void prv_apps_color_menu_select_short(OptionMenu *option_menu, int selection, void *context) {
  if (selection == 0){
    /* Default option selected -> restore default color. */
    shell_prefs_set_apps_menu_highlight_color(DEFAULT_APPS_HIGHLIGHT_COLOR);
  }
  else if (selection == ARRAY_LENGTH(s_color_definitions_short) - 1) {
    /* "Show All..." -> open full color menu. */
    prv_push_apps_color_menu((SettingsThemesData *)context);
    return;
  }
  else{
    shell_prefs_set_apps_menu_highlight_color(s_color_definitions[selection].value);
  }
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_option_apps_menu_selection_will_change_short(OptionMenu *option_menu,
                                             uint16_t new_row,
                                             uint16_t old_row,
                                             void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color = s_color_definitions_short[new_row].value;
  if (new_row >= ARRAY_LENGTH(s_color_definitions_short) - 1) {
    /* "Show All..." selected -> preview current saved color. */
    option_menu_set_highlight_colors(option_menu, shell_prefs_get_apps_menu_highlight_color(), PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
  else if (color.argb != GColorClear.argb) {
    option_menu_set_highlight_colors(option_menu, color, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
  else {
    option_menu_set_highlight_colors(option_menu, DEFAULT_APPS_HIGHLIGHT_COLOR, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
}


static void prv_push_apps_color_menu_short(SettingsThemesData *data) {
  const char *title = i18n_noop("Apps Menu Accent");
  int selected = prv_color_to_index(shell_prefs_get_apps_menu_highlight_color(), true, DEFAULT_APPS_HIGHLIGHT_COLOR);
  const char** color_names = prv_get_color_names(true);
  const OptionMenuCallbacks callbacks = {
    .select = prv_apps_color_menu_select_short,
    .selection_will_change = prv_option_apps_menu_selection_will_change_short,
  };
  if (selected < 0) {
    selected = ARRAY_LENGTH(s_color_definitions_short) - 1;
  }
  OptionMenu * const option_menu = settings_option_menu_create(
      title, OptionMenuContentType_SingleLine, selected, &callbacks,
      ARRAY_LENGTH(s_color_definitions_short), true /* icons_enabled */, color_names, data);

  if (option_menu) {
    const bool animated = true;
    if (selected == 0) {
      option_menu_set_highlight_colors(option_menu, DEFAULT_APPS_HIGHLIGHT_COLOR, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    }
    else if (selected == ARRAY_LENGTH(s_color_definitions_short) - 1) {
      // "Show All..." option selected
      option_menu_set_highlight_colors(option_menu, shell_prefs_get_apps_menu_highlight_color(), PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    }
    else {
      option_menu_set_highlight_colors(option_menu, s_color_definitions_short[selected].value, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    }
    app_window_stack_push(&option_menu->window, animated);
  }
}


/////////////////////////////
// Settings Accent Color Settings
/////////////////////////////
static void prv_settings_color_menu_select(OptionMenu *option_menu, int selection, void *context) {
  if (selection == 0){
    /* Default option selected -> restore default color. */
    shell_prefs_set_settings_menu_highlight_color(DEFAULT_SETTINGS_HIGHLIGHT_COLOR);
  }
  else{
    shell_prefs_set_settings_menu_highlight_color(s_color_definitions[selection].value);
  }
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_option_settings_menu_selection_will_change(OptionMenu *option_menu,
                                             uint16_t new_row,
                                             uint16_t old_row,
                                             void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color = s_color_definitions[new_row].value;
  if (color.argb != GColorClear.argb) {
    option_menu_set_highlight_colors(option_menu, color, GColorWhite);
  }
  else {
    option_menu_set_highlight_colors(option_menu, DEFAULT_SETTINGS_HIGHLIGHT_COLOR, GColorWhite);
  }
}

static void prv_push_settings_color_menu(SettingsThemesData *data) {
  const char *title = i18n_noop("Settings Accent");
  int selected = prv_color_to_index(shell_prefs_get_settings_menu_highlight_color(), false, DEFAULT_SETTINGS_HIGHLIGHT_COLOR);
  const char** color_names = prv_get_color_names(false);
  const OptionMenuCallbacks callbacks = {
    .select = prv_settings_color_menu_select,
    .selection_will_change = prv_option_settings_menu_selection_will_change,
  };
  if (selected < 0) {
    WTF;
  }
  OptionMenu * const option_menu = settings_option_menu_create(
      title, OptionMenuContentType_SingleLine, selected, &callbacks,
      ARRAY_LENGTH(s_color_definitions), true /* icons_enabled */, color_names, data);

  if (option_menu) {
    const bool animated = true;
    option_menu_set_normal_colors(option_menu,
                               PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite),
                               PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    if (selected == 0) {
      option_menu_set_highlight_colors(option_menu, DEFAULT_SETTINGS_HIGHLIGHT_COLOR, GColorWhite);
    }
    else {
      option_menu_set_highlight_colors(option_menu, s_color_definitions[selected].value, GColorWhite);
    }
    app_window_stack_push(&option_menu->window, animated);
  }
}


/////////////////////////////
// Short Settings Accent Color Settings
/////////////////////////////
static void prv_settings_color_menu_select_short(OptionMenu *option_menu, int selection, void *context) {
  if (selection == 0){
    /* Default option selected -> restore default color. */
    shell_prefs_set_settings_menu_highlight_color(DEFAULT_SETTINGS_HIGHLIGHT_COLOR);
  }
  else if (selection == ARRAY_LENGTH(s_color_definitions_short) - 1) {
    prv_push_settings_color_menu((SettingsThemesData *)context);
    return;
  }
  else{
    shell_prefs_set_settings_menu_highlight_color(s_color_definitions[selection].value);
  }
}

static void prv_option_settings_menu_selection_will_change_short(OptionMenu *option_menu,
                                             uint16_t new_row,
                                             uint16_t old_row,
                                             void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color = s_color_definitions[new_row].value;
  if (new_row >= ARRAY_LENGTH(s_color_definitions_short) - 1) {
    /* "Show All..." selected -> preview saved color. */
    option_menu_set_highlight_colors(option_menu, shell_prefs_get_settings_menu_highlight_color(), GColorWhite);
  }
  else if (color.argb != GColorClear.argb) {
    option_menu_set_highlight_colors(option_menu, color, GColorWhite);
  }
  else {
    option_menu_set_highlight_colors(option_menu, DEFAULT_SETTINGS_HIGHLIGHT_COLOR, GColorWhite);
  }
}


static void prv_push_settings_color_menu_short(SettingsThemesData *data) {
  const char *title = i18n_noop("Settings Accent");
  int selected = prv_color_to_index(shell_prefs_get_settings_menu_highlight_color(), true, DEFAULT_SETTINGS_HIGHLIGHT_COLOR);
  const char** color_names = prv_get_color_names(true);
  const OptionMenuCallbacks callbacks = {
    .select = prv_settings_color_menu_select_short,
    .selection_will_change = prv_option_settings_menu_selection_will_change_short,
  };
  if (selected < 0) {
    selected = ARRAY_LENGTH(s_color_definitions_short) - 1;
  }
  OptionMenu * const option_menu = settings_option_menu_create(
      title, OptionMenuContentType_SingleLine, selected, &callbacks,
      ARRAY_LENGTH(s_color_definitions_short), true /* icons_enabled */, color_names, data);

  if (option_menu) {
    const bool animated = true;
    option_menu_set_normal_colors(option_menu,
                               PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite),
                               PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    if (selected == 0) {
      option_menu_set_highlight_colors(option_menu, DEFAULT_SETTINGS_HIGHLIGHT_COLOR, GColorWhite);
    }
    else if (selected == ARRAY_LENGTH(s_color_definitions_short) - 1) {
      // "Show All..." option selected
      option_menu_set_highlight_colors(option_menu, shell_prefs_get_settings_menu_highlight_color(), GColorWhite);
    }
    else {
      option_menu_set_highlight_colors(option_menu, s_color_definitions_short[selected].value, GColorWhite);
    }
    app_window_stack_push(&option_menu->window, animated);
  }
}



static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
#if PBL_COLOR
  SettingsThemesData *data = (SettingsThemesData *)context;
  switch ((ThemesMenuIndex)row) {
    case ThemesMenuIndex_Apps:
      prv_push_apps_color_menu_short(data);
      goto done;
    case ThemesMenuIndex_Settings:
      prv_push_settings_color_menu_short(data);
      goto done;
    case ThemesMenuIndexCount:
      break;
  }
  WTF;
done:
  settings_menu_reload_data(SettingsMenuItemThemes);
#else
  WTF;
#endif
}

static Window *prv_create_settings_window(void) {
#if PBL_COLOR
  SettingsThemesData *data = app_malloc_check(sizeof(*data));

  *data = (SettingsThemesData) {
    .callbacks = {
      .deinit = prv_deinit_cb,
      .draw_row = prv_draw_row_cb,
      .select_click = prv_select_click_cb,
      .num_rows = prv_num_rows_cb,
    }
  };

  return settings_window_create(SettingsMenuItemThemes, &data->callbacks);
#else 
  WTF;
  return NULL;
#endif
}

static Window *prv_init(void) {
  return prv_create_settings_window();
}


const SettingsModuleMetadata *settings_themes_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    /// Title of the Themes Settings submenu in Settings
    .name = i18n_noop("Themes"),
    .init = prv_init,
  };

  return &s_module_info;
}
