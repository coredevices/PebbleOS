/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! This file displays the main Quick Launch menu that is found in our settings menu
//! It allows the feature to be enabled or for an app to be set
//! The list of apps that the user can choose from is found in settings_quick_launch_app_menu.c
//! This file is also responsible for saving / storing the uuid of each quichlaunch app as well as
//! whether or not the quicklaunch app is enabled.

#include "menu.h"
#include "quick_launch.h"
#include "quick_launch_app_menu.h"
#include "quick_launch_setup_menu.h"
#include "window.h"

#include "applib/app.h"
#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/ui/window_stack.h"
#include "apps/system_app_ids.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_menu_data_source.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "shell/normal/quick_launch.h"
#include "system/passert.h"
#include "system/status_codes.h"

#define NUM_ROWS (NUM_BUTTONS - 1)  // 4 buttons - back button

typedef enum {
  ROW_UP = 0,
  ROW_SELECT,
  ROW_DOWN,
} QuickLaunchTwoClicksRow;

typedef struct QuickLaunchTwoClicksData {
  SettingsCallbacks callbacks;
  char app_names[NUM_ROWS][APP_NAME_SIZE_BYTES];
  ButtonId first_button;
  bool first_button_was_tap;
} QuickLaunchTwoClicksData;

static const char *s_row_titles[NUM_ROWS] = {
  /// Shown in Quick Launch Settings as the title of the tap up button option.
  [ROW_UP]       = i18n_noop("Tap Up"),
  /// Shown in Quick Launch Settings as the title of the tap down button option.
  [ROW_SELECT]     = i18n_noop("Tap Center"),
  /// Shown in Quick Launch Settings as the title of the hold up button quick launch option.
  [ROW_DOWN]      = i18n_noop("Tap Down"),
};

static void prv_get_subtitle_string(AppInstallId app_id, QuickLaunchTwoClicksData *data,
                                    char *buffer, uint8_t buf_len) {
  if (app_id == INSTALL_ID_INVALID) {
    /// Shown in Quick Launch Settings when the button is disabled.
    i18n_get_with_buffer("Disabled", buffer, buf_len);
    return;
  } else {
    AppInstallEntry entry;
    if (app_install_get_entry_for_install_id(app_id, &entry)) {
      strncpy(buffer, entry.name, buf_len);
      buffer[buf_len - 1] = '\0';
      return;
    }
  }
  // if failed both, set as empty string
  buffer[0] = '\0';
}

// Filter List Callbacks
////////////////////////
static void prv_deinit_cb(SettingsCallbacks *context) {
  QuickLaunchTwoClicksData *data = (QuickLaunchTwoClicksData *) context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_update_app_names(QuickLaunchTwoClicksData *data) {
  if (data->first_button_was_tap) {
    prv_get_subtitle_string(quick_launch_two_clicks_tap_get_app(data->first_button, BUTTON_ID_UP), data,
                            data->app_names[ROW_UP], APP_NAME_SIZE_BYTES);
    prv_get_subtitle_string(quick_launch_two_clicks_tap_get_app(data->first_button, BUTTON_ID_SELECT), data,
                            data->app_names[ROW_SELECT], APP_NAME_SIZE_BYTES);
    prv_get_subtitle_string(quick_launch_two_clicks_tap_get_app(data->first_button, BUTTON_ID_DOWN), data,
                            data->app_names[ROW_DOWN], APP_NAME_SIZE_BYTES);
  } else {
    prv_get_subtitle_string(quick_launch_two_clicks_get_app(data->first_button, BUTTON_ID_UP), data,
                            data->app_names[ROW_UP], APP_NAME_SIZE_BYTES);
    prv_get_subtitle_string(quick_launch_two_clicks_get_app(data->first_button, BUTTON_ID_SELECT), data,
                            data->app_names[ROW_SELECT], APP_NAME_SIZE_BYTES);
    prv_get_subtitle_string(quick_launch_two_clicks_get_app(data->first_button, BUTTON_ID_DOWN), data,
                            data->app_names[ROW_DOWN], APP_NAME_SIZE_BYTES);
  }
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  QuickLaunchTwoClicksData *data = (QuickLaunchTwoClicksData *)context;
  PBL_ASSERTN(row < NUM_ROWS);
  const char *title = i18n_get(s_row_titles[row], data);
  char *subtitle_buf = data->app_names[row];
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle_buf, NULL);
}

static uint16_t prv_get_initial_selection_cb(SettingsCallbacks *context) {
  // If launched by quick launch, select the row of the button pressed, otherwise default to 0
  if (app_launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    ButtonId button = app_launch_button();
    // Map button to hold row (quick launch is always hold/long press)
    switch (button) {
      case BUTTON_ID_UP:     return ROW_UP;
      case BUTTON_ID_SELECT: return ROW_SELECT;
      case BUTTON_ID_DOWN:   return ROW_DOWN;
      default: break;
    }
  }
  return 0;
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  PBL_ASSERTN(row < NUM_ROWS);
  QuickLaunchTwoClicksData *data = (QuickLaunchTwoClicksData *)context;
  ButtonId button;
  
  switch (row) {
    case ROW_UP:
      button = BUTTON_ID_UP;
      break;
    case ROW_SELECT:
      button = BUTTON_ID_SELECT;
      break;
    case ROW_DOWN:
      button = BUTTON_ID_DOWN;
      break;
    default:
      return;
  }
  
  // Here, we need to display a window similar to 'settings_quick_launch_app_menu', dismissing the 2-Clicks app
  quick_launch_two_clicks_app_menu_window_push(data->first_button, data->first_button_was_tap, button);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return NUM_ROWS;
}

static void prv_appear(SettingsCallbacks *context) {
  QuickLaunchTwoClicksData *data = (QuickLaunchTwoClicksData *)context;
  prv_update_app_names(data);
}

Window *settings_quick_launch_two_clicks_init(ButtonId first_button, bool first_button_was_tap) {
  QuickLaunchTwoClicksData *data = app_malloc_check(sizeof(*data));
  *data = (QuickLaunchTwoClicksData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .get_initial_selection = prv_get_initial_selection_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear,
  };
  data->first_button = first_button;
  data->first_button_was_tap = first_button_was_tap;

  return settings_window_create(SettingsMenuItemQuickLaunch, &data->callbacks);
}
