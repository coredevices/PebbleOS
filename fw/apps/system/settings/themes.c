/* SPDX-FileCopyrightText: 2025 Elad Dvash */
/* SPDX-License-Identifier: Apache-2.0 */

#include "themes.h"

#include "menu.h"
#include "option_menu.h"
#include "window.h"

#include <string.h>

#include <pbl/kernel/fatal.h>
#include <pbl/logging/logging.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/size.h>

#include <applib/graphics/gtypes.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/menu_layer.h>
#include <applib/ui/option_menu_window.h>
#include <kernel/pbl_malloc.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

#ifdef CONFIG_THEMING

#define DEFAULT_THEME_PRIMARY_COLOR   GColorVividCerulean
#define DEFAULT_THEME_SECONDARY_COLOR GColorCobaltBlue

#define THEME_PRESET_DEFAULT_LIGHT 0
#define THEME_PRESET_DEFAULT_DARK  1
#define THEME_PRESET_COUNT         2

typedef struct ColorDefinition {
  const char *name;
  GColor color;
} ColorDefinition;

static const ColorDefinition s_accent_color_definitions[] = {
  {i18n_noop("Default"), GColorClear},
  {i18n_noop("Red"), GColorSunsetOrange},
  {i18n_noop("Orange"), GColorChromeYellow},
  {i18n_noop("Yellow"), GColorYellow},
  {i18n_noop("Green"), GColorGreen},
  {i18n_noop("Islamic Green"), GColorIslamicGreen},
  {i18n_noop("Cyan"), GColorCyan},
  {i18n_noop("Tiffany Blue"), GColorTiffanyBlue},
  {i18n_noop("Light Blue"), GColorVividCerulean},
  {i18n_noop("Cobalt Blue"), GColorCobaltBlue},
  {i18n_noop("Oxford Blue"), GColorOxfordBlue},
  {i18n_noop("Royal Blue"), GColorVeryLightBlue},
  {i18n_noop("Purple"), GColorLavenderIndigo},
  {i18n_noop("Imperial Purple"), GColorImperialPurple},
  {i18n_noop("Magenta"), GColorMagenta},
  {i18n_noop("Jazzberry Jam"), GColorJazzberryJam},
  {i18n_noop("Pink"), GColorBrilliantRose},
};

static const ColorDefinition s_bg_color_definitions[] = {
  {i18n_noop("White"), GColorWhite},
  {i18n_noop("Black"), GColorBlack},
  {i18n_noop("Light Gray"), GColorLightGray},
  {i18n_noop("Dark Gray"), GColorDarkGray},
  {i18n_noop("Oxford Blue"), GColorOxfordBlue},
  {i18n_noop("Bulgarian Rose"), GColorBulgarianRose},
  {i18n_noop("Imperial Purple"), GColorImperialPurple},
  {i18n_noop("Dark Green"), GColorDarkGreen},
  {i18n_noop("Pastel Yellow"), GColorPastelYellow},
  {i18n_noop("Mint Green"), GColorMintGreen},
  {i18n_noop("Celeste"), GColorCeleste},
  {i18n_noop("Baby Blue Eyes"), GColorBabyBlueEyes},
  {i18n_noop("Lavender"), GColorRichBrilliantLavender},
  {i18n_noop("Melon"), GColorMelon},
};

enum SettingsThemesItem {
  SettingsThemesItemTheme,
  SettingsThemesItemPrimaryColor,
  SettingsThemesItemSecondaryColor,
  SettingsThemesItemBackgroundColor,
  SettingsThemesItemAppColorMode,
  NumSettingsThemesItems
};

typedef struct SettingsThemesData {
  SettingsCallbacks callbacks;
  const char *accent_labels[ARRAY_LENGTH(s_accent_color_definitions)];
  const char *bg_labels[ARRAY_LENGTH(s_bg_color_definitions)];
} SettingsThemesData;

static int prv_find_color_index(const ColorDefinition *defs, size_t count, GColor color,
                                GColor default_color) {
  for (size_t i = 0; i < count; i++) {
    if (!gcolor_is_transparent(defs[i].color) && gcolor_equal(color, defs[i].color)) {
      return (int)i;
    }
  }
  if (gcolor_is_transparent(color) || gcolor_equal(color, default_color)) {
    return 0;
  }
  return -1;
}

static const char *prv_get_color_name(const ColorDefinition *defs, size_t count, GColor color,
                                      GColor default_color) {
  for (size_t i = 0; i < count; i++) {
    if (!gcolor_is_transparent(defs[i].color) && gcolor_equal(color, defs[i].color)) {
      return defs[i].name;
    }
  }
  if (gcolor_is_transparent(color) || gcolor_equal(color, default_color)) {
    return i18n_noop("Default");
  }
  return i18n_noop("Custom");
}

static bool prv_is_default_dark_theme(void) {
  return (shell_prefs_get_theme_bg_resource_id() == 0) &&
         gcolor_equal(shell_prefs_get_theme_background_color(), GColorBlack) &&
         gcolor_equal(shell_prefs_get_theme_highlight_color(), GColorCobaltBlue) &&
         gcolor_equal(shell_prefs_get_theme_secondary_color(), GColorOxfordBlue);
}

static bool prv_is_default_light_theme(void) {
  return shell_prefs_is_theme_background_default() &&
         gcolor_equal(shell_prefs_get_theme_highlight_color(), DEFAULT_THEME_PRIMARY_COLOR) &&
         gcolor_equal(shell_prefs_get_theme_secondary_color(), DEFAULT_THEME_SECONDARY_COLOR);
}

static const char *prv_get_active_theme_subtitle(SettingsThemesData *data) {
  if (prv_is_default_dark_theme()) {
    return i18n_noop("Default Dark");
  }
  if (prv_is_default_light_theme()) {
    return i18n_noop("Default Light");
  }
  return i18n_noop("Custom");
}

static uint16_t prv_theme_menu_get_num_rows(OptionMenu *option_menu, void *context) {
  return THEME_PRESET_COUNT;
}

static void prv_theme_menu_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                                    const GRect *cell_frame, uint32_t row, bool selected,
                                    void *context) {
  SettingsThemesData *data = context;
  const char *title = nullptr;
  if (row == THEME_PRESET_DEFAULT_LIGHT) {
    title = i18n_get("Default Light", data);
  } else if (row == THEME_PRESET_DEFAULT_DARK) {
    title = i18n_get("Default Dark", data);
  }
  option_menu_system_draw_row(option_menu, ctx, cell_layer, cell_frame, title, selected, context);
}

static void prv_theme_menu_select(OptionMenu *option_menu, int selection, void *context) {
  if (selection == THEME_PRESET_DEFAULT_LIGHT) {
    system_theme_reset_to_default(false /* dark_mode */);
  } else if (selection == THEME_PRESET_DEFAULT_DARK) {
    system_theme_reset_to_default(true /* dark_mode */);
  }
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_theme_menu_push(SettingsThemesData *data) {
  OptionMenu *option_menu = option_menu_create();
  if (!option_menu) {
    return;
  }

  int choice = OPTION_MENU_CHOICE_NONE;
  if (prv_is_default_dark_theme()) {
    choice = THEME_PRESET_DEFAULT_DARK;
  } else if (prv_is_default_light_theme()) {
    choice = THEME_PRESET_DEFAULT_LIGHT;
  }

  SystemThemeColors colors;
  system_theme_get_colors(&colors);
  const OptionMenuConfig config = {
    .title = i18n_get("Theme", data),
    .choice = choice,
    .status_colors = {colors.secondary_color, colors.secondary_text_color},
    .highlight_colors = {colors.primary_color, colors.primary_text_color},
    .icons_enabled = true,
  };
  option_menu_configure(option_menu, &config);
  option_menu_set_theme_colors(option_menu, &colors);
  option_menu_set_callbacks(option_menu,
                            &(OptionMenuCallbacks){
                              .draw_row = prv_theme_menu_draw_row,
                              .get_num_rows = prv_theme_menu_get_num_rows,
                              .select = prv_theme_menu_select,
                            },
                            data);
  app_window_stack_push(&option_menu->window, true /* animated */);
}

// Primary Color Menu
//////////////////////

static void prv_primary_color_select(OptionMenu *option_menu, int selection, void *context) {
  GColor color =
      (selection == 0) ? DEFAULT_THEME_PRIMARY_COLOR : s_accent_color_definitions[selection].color;
  shell_prefs_set_theme_highlight_color(color);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_primary_color_selection_will_change(OptionMenu *option_menu, uint16_t new_row,
                                                    uint16_t old_row, void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color =
      (new_row == 0) ? DEFAULT_THEME_PRIMARY_COLOR : s_accent_color_definitions[new_row].color;
  option_menu_set_highlight_colors(option_menu, color, gcolor_legible_over(color));
}

static void prv_primary_color_menu_push(SettingsThemesData *data) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_accent_color_definitions); i++) {
    data->accent_labels[i] = s_accent_color_definitions[i].name;
  }
  int selected =
      prv_find_color_index(s_accent_color_definitions, ARRAY_LENGTH(s_accent_color_definitions),
                           shell_prefs_get_theme_highlight_color(), DEFAULT_THEME_PRIMARY_COLOR);
  if (selected < 0) {
    selected = 0;
  }
  const OptionMenuCallbacks callbacks = {
    .select = prv_primary_color_select,
    .selection_will_change = prv_primary_color_selection_will_change,
  };
  OptionMenu *option_menu =
      settings_option_menu_push(i18n_noop("Primary Color"), OptionMenuContentType_SingleLine,
                                selected, &callbacks, ARRAY_LENGTH(s_accent_color_definitions),
                                true /* icons_enabled */, data->accent_labels, data);
  if (option_menu) {
    const GColor preview_color =
        (selected == 0) ? DEFAULT_THEME_PRIMARY_COLOR : s_accent_color_definitions[selected].color;
    option_menu_set_highlight_colors(option_menu, preview_color,
                                     gcolor_legible_over(preview_color));
  }
}

// Secondary Color Menu
////////////////////////

static void prv_secondary_color_select(OptionMenu *option_menu, int selection, void *context) {
  GColor color = (selection == 0) ? DEFAULT_THEME_SECONDARY_COLOR
                                  : s_accent_color_definitions[selection].color;
  shell_prefs_set_theme_secondary_color(color);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_secondary_color_selection_will_change(OptionMenu *option_menu, uint16_t new_row,
                                                      uint16_t old_row, void *context) {
  if (new_row == old_row) {
    return;
  }
  GColor color =
      (new_row == 0) ? DEFAULT_THEME_SECONDARY_COLOR : s_accent_color_definitions[new_row].color;
  option_menu_set_status_colors(option_menu, color, gcolor_legible_over(color));
}

static void prv_secondary_color_menu_push(SettingsThemesData *data) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_accent_color_definitions); i++) {
    data->accent_labels[i] = s_accent_color_definitions[i].name;
  }
  int selected =
      prv_find_color_index(s_accent_color_definitions, ARRAY_LENGTH(s_accent_color_definitions),
                           shell_prefs_get_theme_secondary_color(), DEFAULT_THEME_SECONDARY_COLOR);
  if (selected < 0) {
    selected = 0;
  }
  const OptionMenuCallbacks callbacks = {
    .select = prv_secondary_color_select,
    .selection_will_change = prv_secondary_color_selection_will_change,
  };
  OptionMenu *option_menu =
      settings_option_menu_push(i18n_noop("Secondary Color"), OptionMenuContentType_SingleLine,
                                selected, &callbacks, ARRAY_LENGTH(s_accent_color_definitions),
                                true /* icons_enabled */, data->accent_labels, data);
  if (option_menu) {
    const GColor preview_color = (selected == 0) ? DEFAULT_THEME_SECONDARY_COLOR
                                                 : s_accent_color_definitions[selected].color;
    option_menu_set_status_colors(option_menu, preview_color, gcolor_legible_over(preview_color));
  }
}

// Background Color Menu
/////////////////////////

static void prv_bg_color_select(OptionMenu *option_menu, int selection, void *context) {
  const Uuid invalid_uuid = UUID_INVALID;
  shell_prefs_set_theme_active_uuid(&invalid_uuid);
  shell_prefs_set_theme_bg_resource_id(0);
  shell_prefs_set_theme_text_color(GColorClear);
  const GColor color = (selection == 0) ? GColorClear : s_bg_color_definitions[selection].color;
  shell_prefs_set_theme_background_color(color);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_bg_color_selection_will_change(OptionMenu *option_menu, uint16_t new_row,
                                               uint16_t old_row, void *context) {
  if (new_row == old_row) {
    return;
  }
  const GColor color = s_bg_color_definitions[new_row].color;
  option_menu_set_normal_colors(option_menu, color, gcolor_legible_over(color));
}

static void prv_bg_color_menu_push(SettingsThemesData *data) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_bg_color_definitions); i++) {
    data->bg_labels[i] = s_bg_color_definitions[i].name;
  }
  int selected = OPTION_MENU_CHOICE_NONE;
  if (shell_prefs_get_theme_bg_resource_id() == 0) {
    selected = prv_find_color_index(s_bg_color_definitions, ARRAY_LENGTH(s_bg_color_definitions),
                                    shell_prefs_get_theme_background_color(), GColorWhite);
  }
  const OptionMenuCallbacks callbacks = {
    .select = prv_bg_color_select,
    .selection_will_change = prv_bg_color_selection_will_change,
  };
  OptionMenu *option_menu = settings_option_menu_push(
      i18n_noop("Background"), OptionMenuContentType_SingleLine, (selected >= 0) ? selected : 0,
      &callbacks, ARRAY_LENGTH(s_bg_color_definitions), true /* icons_enabled */, data->bg_labels,
      data);
  if (option_menu && selected >= 0) {
    const GColor preview_color = s_bg_color_definitions[selected].color;
    option_menu_set_normal_colors(option_menu, preview_color, gcolor_legible_over(preview_color));
  }
}

// Themes Submenu Callbacks
////////////////////////////

static void prv_themes_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsThemesData *data = (SettingsThemesData *)context;
  switch (row) {
    case SettingsThemesItemTheme:
      prv_theme_menu_push(data);
      break;
    case SettingsThemesItemPrimaryColor:
      prv_primary_color_menu_push(data);
      break;
    case SettingsThemesItemSecondaryColor:
      prv_secondary_color_menu_push(data);
      break;
    case SettingsThemesItemBackgroundColor:
      prv_bg_color_menu_push(data);
      break;
    case SettingsThemesItemAppColorMode: {
      const ThemeAppColorMode current = shell_prefs_get_theme_app_color_mode();
      const ThemeAppColorMode next = (current == ThemeAppColorMode_BackgroundOnly)
                                         ? ThemeAppColorMode_ColorsAndBackground
                                         : ThemeAppColorMode_BackgroundOnly;
      shell_prefs_set_theme_app_color_mode(next);
      break;
    }
    default:
      pbl_panic();
  }
  settings_menu_reload_data(SettingsMenuItemThemes);
  settings_menu_mark_dirty(SettingsMenuItemThemes);
}

static void prv_themes_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                   const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsThemesData *data = (SettingsThemesData *)context;
  const char *title = nullptr;
  const char *subtitle = nullptr;
  switch (row) {
    case SettingsThemesItemTheme:
      title = i18n_noop("Theme");
      subtitle = prv_get_active_theme_subtitle(data);
      break;
    case SettingsThemesItemPrimaryColor:
      title = i18n_noop("Primary Color");
      subtitle =
          prv_get_color_name(s_accent_color_definitions, ARRAY_LENGTH(s_accent_color_definitions),
                             shell_prefs_get_theme_highlight_color(), DEFAULT_THEME_PRIMARY_COLOR);
      break;
    case SettingsThemesItemSecondaryColor:
      title = i18n_noop("Secondary Color");
      subtitle = prv_get_color_name(
          s_accent_color_definitions, ARRAY_LENGTH(s_accent_color_definitions),
          shell_prefs_get_theme_secondary_color(), DEFAULT_THEME_SECONDARY_COLOR);
      break;
    case SettingsThemesItemBackgroundColor:
      title = i18n_noop("Background");
      if (shell_prefs_get_theme_bg_resource_id() != 0) {
        subtitle = i18n_noop("Theme Image");
      } else {
        subtitle = prv_get_color_name(s_bg_color_definitions, ARRAY_LENGTH(s_bg_color_definitions),
                                      shell_prefs_get_theme_background_color(), GColorWhite);
      }
      break;
    case SettingsThemesItemAppColorMode:
      title = i18n_noop("Default Apps");
      subtitle = (shell_prefs_get_theme_app_color_mode() == ThemeAppColorMode_BackgroundOnly)
                     ? i18n_noop("Background Only")
                     : i18n_noop("Colors & Background");
      break;
    default:
      pbl_panic();
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), nullptr);
}

static uint16_t prv_themes_num_rows_cb(SettingsCallbacks *context) {
  return NumSettingsThemesItems;
}

static void prv_themes_deinit_cb(SettingsCallbacks *context) {
  SettingsThemesData *data = (SettingsThemesData *)context;
  i18n_free_all(data);
  app_free(data);
}

#endif // CONFIG_THEMING

static Window *prv_init(void) {
#ifdef CONFIG_THEMING
  SettingsThemesData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsThemesData){};
  data->callbacks = (SettingsCallbacks){
    .deinit = prv_themes_deinit_cb,
    .draw_row = prv_themes_draw_row_cb,
    .select_click = prv_themes_select_click_cb,
    .num_rows = prv_themes_num_rows_cb,
  };
  return settings_window_create(SettingsMenuItemThemes, &data->callbacks);
#else
  pbl_panic();
  return nullptr;
#endif
}

const SettingsModuleMetadata *settings_themes_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    /// Title of the Themes Settings submenu in Settings
    .name = i18n_noop("Themes"),
    .init = prv_init,
  };

  return &s_module_info;
}
