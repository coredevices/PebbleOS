/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! This file displays the main Quick Launch menu that is found in our settings menu
//! It allows the feature to be enabled or for an action or app to be set
//! The list of targets that the user can choose from is found in quick_launch_app_menu.c
//! This file is also responsible for saving / storing the uuid of each quicklaunch app as well as
//! whether or not the quicklaunch app is enabled.

#include "quick_launch.h"

#include "menu.h"
#include "option_menu.h"
#include "quick_launch_app_menu.h"
#include "quick_launch_slot.h"
#include "quick_launch_setup_menu.h"
#include "window.h"

#include <pbl/kernel/assert.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/size.h>

#include <applib/app_launch_button.h>
#include <applib/app_launch_reason.h>
#include <applib/ui/window_stack.h>
#include <kernel/pbl_malloc.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_menu_data_source.h>
#include <resource/resource_ids.auto.h>
#include <shell/normal/quick_launch.h>

typedef struct {
  const char *title;
  QuickLaunchSlot slot;
} QuickLaunchMenuRow;

static const QuickLaunchMenuRow s_rows[] = {
  /// Shown in Quick Launch Settings as the title of the tap up button option.
  {i18n_noop("Tap Up"), {.type = QuickLaunchSlotTap, .button = BUTTON_ID_UP}},
  /// Shown in Quick Launch Settings as the title of the tap down button option.
  {i18n_noop("Tap Down"), {.type = QuickLaunchSlotTap, .button = BUTTON_ID_DOWN}},
  /// Shown in Quick Launch Settings as the title of the hold up button option.
  {i18n_noop("Hold Up"), {.type = QuickLaunchSlotHold, .button = BUTTON_ID_UP}},
  /// Shown in Quick Launch Settings as the title of the hold center button option.
  {i18n_noop("Hold Center"), {.type = QuickLaunchSlotHold, .button = BUTTON_ID_SELECT}},
  /// Shown in Quick Launch Settings as the title of the hold down button option.
  {i18n_noop("Hold Down"), {.type = QuickLaunchSlotHold, .button = BUTTON_ID_DOWN}},
  /// Shown in Quick Launch Settings as the title of the hold back button option.
  {i18n_noop("Hold Back"), {.type = QuickLaunchSlotHold, .button = BUTTON_ID_BACK}},
#ifdef CONFIG_TOUCH
  /// Shown in Quick Launch Settings as the title of the swipe up option.
  {i18n_noop("Swipe Up"), {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Up}},
  /// Shown in Quick Launch Settings as the title of the swipe down option.
  {i18n_noop("Swipe Down"), {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Down}},
  /// Shown in Quick Launch Settings as the title of the swipe left option.
  {i18n_noop("Swipe Left"), {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Left}},
  /// Shown in Quick Launch Settings as the title of the swipe right option.
  {i18n_noop("Swipe Right"), {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Right}},
#endif
};

#define NUM_ROWS ARRAY_LENGTH(s_rows)

typedef enum {
  CATEGORY_ROW_ACTIONS = 0,
  CATEGORY_ROW_APPS,

  CategoryRowCount,
} QuickLaunchCategoryRow;

typedef struct QuickLaunchData {
  SettingsCallbacks callbacks;
  char app_names[NUM_ROWS][APP_NAME_SIZE_BYTES];
  QuickLaunchSlot slot;
} QuickLaunchData;

static const char *s_category_row_titles[CategoryRowCount] = {
  /// Shown in Quick Launch Settings as the title of the list of actions.
  [CATEGORY_ROW_ACTIONS] = i18n_noop("Actions"),
  /// Shown in Quick Launch Settings as the title of the list of apps.
  [CATEGORY_ROW_APPS] = i18n_noop("Apps"),
};

static void prv_get_subtitle_string(AppInstallId app_id, QuickLaunchData *data, char *buffer,
                                    uint8_t buf_len) {
  if (app_id == INSTALL_ID_INVALID) {
    /// Shown in Quick Launch Settings when no action or app is bound to the button.
    i18n_get_with_buffer("Unassigned", buffer, buf_len);
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
  QuickLaunchData *data = (QuickLaunchData *)context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_update_app_names(QuickLaunchData *data) {
  for (size_t row = 0; row < NUM_ROWS; ++row) {
    prv_get_subtitle_string(quick_launch_slot_get_app(s_rows[row].slot), data,
                            data->app_names[row], APP_NAME_SIZE_BYTES);
  }
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx, const Layer *cell_layer,
                            uint16_t row, bool selected) {
  QuickLaunchData *data = (QuickLaunchData *)context;
  PBL_ASSERTN(row < NUM_ROWS);
  const char *title = i18n_get(s_rows[row].title, data);
  char *subtitle_buf = data->app_names[row];
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle_buf, nullptr);
}

static uint16_t prv_get_initial_selection_cb(SettingsCallbacks *context) {
  if (app_launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    const AppQuickLaunchAction action = app_launch_get_quick_launch_action();
    const ButtonId button = app_launch_button();
    for (size_t row = 0; row < NUM_ROWS; ++row) {
      const QuickLaunchSlot slot = s_rows[row].slot;
      if (action == APP_QUICK_LAUNCH_ACTION_HOLD && slot.type == QuickLaunchSlotHold &&
          slot.button == button) {
        return row;
      }
#ifdef CONFIG_TOUCH
      if (action == APP_QUICK_LAUNCH_ACTION_SWIPE && slot.type == QuickLaunchSlotSwipe &&
          ((slot.direction == SwipeDirection_Up && button == BUTTON_ID_DOWN) ||
           (slot.direction == SwipeDirection_Down && button == BUTTON_ID_UP) ||
           (slot.direction == SwipeDirection_Left && button == BUTTON_ID_SELECT) ||
           (slot.direction == SwipeDirection_Right && button == BUTTON_ID_BACK))) {
        return row;
      }
#endif
    }
  }
  return 0;
}

//! The row to start on, which is the category the slot is currently bound to.
static QuickLaunchCategoryRow prv_get_category_row(QuickLaunchData *data) {
  const AppInstallId app_id = quick_launch_slot_get_app(data->slot);
  AppInstallEntry entry;
  if ((app_id == INSTALL_ID_INVALID) || !app_install_get_entry_for_install_id(app_id, &entry)) {
    return CATEGORY_ROW_ACTIONS;
  }
  return app_install_entry_is_quick_launch_visible_only(&entry) ? CATEGORY_ROW_ACTIONS
                                                                : CATEGORY_ROW_APPS;
}

static void prv_category_menu_select(OptionMenu *option_menu, int selection, void *context) {
  QuickLaunchData *data = settings_option_menu_get_context(context);
  const QuickLaunchMenuCategory category = (selection == CATEGORY_ROW_ACTIONS)
                                               ? QuickLaunchMenuCategoryActions
                                               : QuickLaunchMenuCategoryApps;
  quick_launch_app_menu_window_push(data->slot, category, &option_menu->window);
}

static void prv_category_menu_push(QuickLaunchData *data) {
  const OptionMenuCallbacks callbacks = {
    .select = prv_category_menu_select,
  };
  settings_option_menu_push(i18n_noop("Quick Launch"), OptionMenuContentType_SingleLine,
                            prv_get_category_row(data), &callbacks, CategoryRowCount,
                            false /* icons_enabled */, s_category_row_titles, data);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  PBL_ASSERTN(row < NUM_ROWS);

  QuickLaunchData *data = (QuickLaunchData *)context;
  data->slot = s_rows[row].slot;
  prv_category_menu_push(data);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return NUM_ROWS;
}

static void prv_appear(SettingsCallbacks *context) {
  QuickLaunchData *data = (QuickLaunchData *)context;
  prv_update_app_names(data);
}

static Window *prv_init(void) {
  QuickLaunchData *data = app_malloc_check(sizeof(*data));
  *data = (QuickLaunchData){};

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .get_initial_selection = prv_get_initial_selection_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear,
  };

  return settings_window_create(SettingsMenuItemQuickLaunch, &data->callbacks);
}

const SettingsModuleMetadata *settings_quick_launch_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    /// Title of the Quick Launch Settings submenu in Settings
    .name = i18n_noop("Quick Launch"),
    .init = prv_init,
  };

  return &s_module_info;
}
