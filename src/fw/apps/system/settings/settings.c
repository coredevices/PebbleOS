/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings.h"
#include "menu.h"
#include "window.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/status_bar_layer.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "shell/prefs.h"
#include "util/size.h"

#define SETTINGS_CATEGORY_MENU_CELL_UNFOCUSED_ROUND_VERTICAL_PADDING 14

// Icon resource IDs for each settings menu item (RESOURCE_ID_INVALID means no icon)
static const uint32_t SETTINGS_MENU_ICON_RESOURCES[SettingsMenuItem_Count] = {
  [SettingsMenuItemBluetooth] = RESOURCE_ID_SETTINGS_MENU_ICON_BLUETOOTH,
  [SettingsMenuItemNotifications] = RESOURCE_ID_SETTINGS_MENU_ICON_NOTIFICATIONS,
#if CAPABILITY_HAS_VIBE_SCORES
  [SettingsMenuItemVibrations] = RESOURCE_ID_SETTINGS_MENU_ICON_VIBRATIONS,
#endif
  [SettingsMenuItemQuietTime] = RESOURCE_ID_SETTINGS_MENU_ICON_QUIET_TIME,
#if CAPABILITY_HAS_TIMELINE_PEEK
  [SettingsMenuItemTimeline] = RESOURCE_ID_SETTINGS_MENU_ICON_TIMELINE,
#endif
  [SettingsMenuItemQuickLaunch] = RESOURCE_ID_SETTINGS_MENU_ICON_QUICK_LAUNCH,
  [SettingsMenuItemDateTime] = RESOURCE_ID_SETTINGS_MENU_ICON_DATE_TIME,
  [SettingsMenuItemDisplay] = RESOURCE_ID_SETTINGS_MENU_ICON_DISPLAY,
#if CAPABILITY_HAS_HEALTH_TRACKING
  [SettingsMenuItemHealth] = RESOURCE_ID_SETTINGS_MENU_ICON_HEALTH,
#endif
#if PBL_COLOR
  [SettingsMenuItemThemes] = RESOURCE_ID_SETTINGS_MENU_ICON_THEMES,
#endif
  [SettingsMenuItemActivity] = RESOURCE_ID_SETTINGS_MENU_ICON_BACKGROUND_APP,
  [SettingsMenuItemSystem] = RESOURCE_ID_SETTINGS_MENU_ICON_SYSTEM,
};

typedef struct {
  Window window;
  StatusBarLayer status_layer;
  MenuLayer menu_layer;
  GBitmap *icons[SettingsMenuItem_Count];
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
  GColor highlight_bg = shell_prefs_get_theme_highlight_color();
  menu_layer_set_highlight_colors(&(data->menu_layer),
                                highlight_bg,
                                gcolor_legible_over(highlight_bg));
  menu_layer_set_scroll_wrap_around(&(data->menu_layer),
                                shell_prefs_get_menu_scroll_wrap_around_enable());
  menu_layer_set_scroll_vibe_on_wrap(&(data->menu_layer),
                                shell_prefs_get_menu_scroll_vibe_behavior() == MenuScrollVibeOnWrapAround);
  menu_layer_set_scroll_vibe_on_blocked(&(data->menu_layer),
                                shell_prefs_get_menu_scroll_vibe_behavior() == MenuScrollVibeOnLocked);

  GBitmap *icon = data->icons[cell_index->row];
  menu_cell_basic_draw(ctx, cell_layer, title, NULL, icon);
}

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  settings_menu_push(cell_index->row);
}

static int16_t prv_get_cell_height_callback(MenuLayer *menu_layer,
                                            MenuIndex *cell_index, void *context) {
#if PBL_ROUND
  PBL_ASSERTN(cell_index->row < SettingsMenuItem_Count);

  const int16_t focused_cell_height = MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT;
  const int16_t unfocused_cell_height =
      ((DISP_ROWS - focused_cell_height) / 2) -
          SETTINGS_CATEGORY_MENU_CELL_UNFOCUSED_ROUND_VERTICAL_PADDING;
  return menu_layer_is_index_selected(menu_layer, cell_index) ? focused_cell_height :
                                                                unfocused_cell_height;
#else
  return menu_cell_small_cell_height();
#endif
}

static int16_t prv_get_separator_height_callback(MenuLayer *menu_layer,
                                                 MenuIndex *cell_index,
                                                 void *context) {
  return 0;
}

static void prv_window_load(Window *window) {
  SettingsAppData *data = window_get_user_data(window);

  // Load icons
  for (size_t i = 0; i < ARRAY_LENGTH(data->icons); i++) {
    if (SETTINGS_MENU_ICON_RESOURCES[i] != RESOURCE_ID_INVALID) {
      data->icons[i] = gbitmap_create_with_resource(SETTINGS_MENU_ICON_RESOURCES[i]);
    } else {
      data->icons[i] = NULL;
    }
  }

  // Create the status bar with title
  StatusBarLayer *status_layer = &data->status_layer;
  status_bar_layer_init(status_layer);
  status_bar_layer_set_title(status_layer, i18n_get("Settings", data), false, false);
  status_bar_layer_set_colors(status_layer, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(status_layer, StatusBarLayerSeparatorModeDotted);
  layer_add_child(&data->window.layer, status_bar_layer_get_layer(status_layer));

  // Create the menu
  GRect bounds = grect_inset(data->window.layer.bounds, (GEdgeInsets) {
    .top = STATUS_BAR_LAYER_HEIGHT,
    .bottom = PBL_IF_RECT_ELSE(0, STATUS_BAR_LAYER_HEIGHT),
  });
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
  GColor highlight_bg = shell_prefs_get_theme_highlight_color();
  menu_layer_set_highlight_colors(menu_layer,
                                  highlight_bg,
                                  gcolor_legible_over(highlight_bg));
  menu_layer_set_click_config_onto_window(menu_layer, &data->window);
  menu_layer_set_scroll_wrap_around(menu_layer, shell_prefs_get_menu_scroll_wrap_around_enable());
  menu_layer_set_scroll_vibe_on_wrap(menu_layer, shell_prefs_get_menu_scroll_vibe_behavior() == MenuScrollVibeOnWrapAround);
  menu_layer_set_scroll_vibe_on_blocked(menu_layer, shell_prefs_get_menu_scroll_vibe_behavior() == MenuScrollVibeOnLocked);

  layer_add_child(&data->window.layer, menu_layer_get_layer(menu_layer));
}

static void prv_window_unload(Window *window) {
  SettingsAppData *data = window_get_user_data(window);

  // Free icons
  for (size_t i = 0; i < ARRAY_LENGTH(data->icons); i++) {
    if (data->icons[i]) {
      gbitmap_destroy(data->icons[i]);
    }
  }

  status_bar_layer_deinit(&data->status_layer);
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
  window_set_background_color(window, GColorWhite);
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
#endif
  };
  return (const PebbleProcessMd*) &s_settings_app;
}
