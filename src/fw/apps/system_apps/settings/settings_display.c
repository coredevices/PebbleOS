/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_display.h"
#include "settings_display_calibration.h"
#include "settings_menu.h"
#include "settings_option_menu.h"
#include "settings_window.h"

#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "drivers/ambient_light.h"
#include "drivers/battery.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "popups/notifications/notification_window.h"
#include "process_state/app_state/app_state.h"
#include "services/common/i18n/i18n.h"
#include "services/common/light.h"
#include "shell/prefs.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef struct SettingsDisplayData {
  SettingsCallbacks callbacks;
  char als_value_buffer[16];  // Buffer for ALS value display
  char backlight_percent_buffer[16];  // Buffer for backlight percentage display
  AppTimer *update_timer;  // Timer for live updating debug values
} SettingsDisplayData;

// Intensity Settings
/////////////////////////////

static const uint32_t s_intensity_values[] = { 5, 25, 45, 70 };

static const char *s_intensity_labels[] = {
    i18n_noop("Low"),
    i18n_noop("Medium"),
    i18n_noop("High"),
    i18n_noop("Blinding")
};

#define BACKLIGHT_SCALE_GRANULARITY 5
// Normalize the result from light get brightness as it sometimes
// will round down/up by a %
static uint8_t prv_get_scaled_brightness(void) {
  return BACKLIGHT_SCALE_GRANULARITY
         * ((backlight_get_intensity_percent() + BACKLIGHT_SCALE_GRANULARITY - 1)
            / BACKLIGHT_SCALE_GRANULARITY);
}

static int prv_intensity_get_selection_index() {
  const uint8_t intensity = prv_get_scaled_brightness();

  // FIXME: PBL-22272 ... We will return idx 0 if someone has an old value for
  // one of the intensity options
  for (int i = 0; i < (int)ARRAY_LENGTH(s_intensity_values); i++) {
    if (s_intensity_values[i] == intensity) {
      return i;
    }
  }
  return 0;
}

static void prv_intensity_menu_select(OptionMenu *option_menu, int selection, void *context) {
  backlight_set_intensity_percent(s_intensity_values[selection]);
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_intensity_menu_push(SettingsDisplayData *data) {
  const int index = prv_intensity_get_selection_index();
  const OptionMenuCallbacks callbacks = {
    .select = prv_intensity_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("INTENSITY"), i18n_noop("Intensity"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks, ARRAY_LENGTH(s_intensity_labels),
      true /* icons_enabled */, s_intensity_labels, data);
}

#if PLATFORM_ASTERIX
// Orientation Settings
/////////////////////////////
static const char *s_display_orientation_labels[] = {
    i18n_noop("Default"),
    i18n_noop("Left-Handed"),
};

static int prv_display_orientation_get_selection_index() {
  return display_orientation_is_left() ? 1 : 0;
}

static void prv_display_orientation_menu_select(OptionMenu *option_menu, int selection,
                                                void *context) {
  if (prv_display_orientation_get_selection_index() == selection) {
    // No change
    app_window_stack_remove(&option_menu->window, true /* animated */);
    return;
  }

  display_orientation_set_left(!display_orientation_is_left());
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_display_orientation_menu_push(SettingsDisplayData *data) {
  const int index = prv_display_orientation_get_selection_index();
  const OptionMenuCallbacks callbacks = {
      .select = prv_display_orientation_menu_select,
  };

  const char *title = i18n_noop("Orientation");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_display_orientation_labels), true,
                            s_display_orientation_labels, data);
}
#endif

// Timeout Settings
/////////////////////////////

static const uint32_t s_timeout_values[] = { 3000, 5000, 8000 };

static const char *s_timeout_labels[] = {
  i18n_noop("3 Seconds"),
  i18n_noop("5 Seconds"),
  i18n_noop("8 Seconds")
};

static int prv_timeout_get_selection_index() {
  uint32_t timeout_ms = backlight_get_timeout_ms();
  for (size_t i = 0; i < ARRAY_LENGTH(s_timeout_values); i++) {
    if (s_timeout_values[i] == timeout_ms) {
      return i;
    }
  }
  return 0;
}

static void prv_timeout_menu_select(OptionMenu *option_menu, int selection, void *context) {
  backlight_set_timeout_ms(s_timeout_values[selection]);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_timeout_menu_push(SettingsDisplayData *data) {
  int index = prv_timeout_get_selection_index();
  const OptionMenuCallbacks callbacks = {
    .select = prv_timeout_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("TIMEOUT"), i18n_noop("Timeout"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks, ARRAY_LENGTH(s_timeout_labels),
      true /* icons_enabled */, s_timeout_labels, data);
}

// Legacy App Mode Settings (Obelix only)
/////////////////////////////
#if PLATFORM_OBELIX
static const char *s_legacy_app_mode_labels[] = {
    i18n_noop("Bezel"),
    i18n_noop("Scaled")
};

static void prv_legacy_app_mode_menu_select(OptionMenu *option_menu, int selection, void *context) {
  shell_prefs_set_legacy_app_render_mode((LegacyAppRenderMode)selection);
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_legacy_app_mode_menu_push(SettingsDisplayData *data) {
  const int index = (int)shell_prefs_get_legacy_app_render_mode();
  const OptionMenuCallbacks callbacks = {
    .select = prv_legacy_app_mode_menu_select,
  };
  const char *title = i18n_noop("Legacy App Display");
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks,
      ARRAY_LENGTH(s_legacy_app_mode_labels),
      false /* icons_enabled */, s_legacy_app_mode_labels, data);
}
#endif

// Menu Callbacks
////////////////////////////

enum SettingsDisplayItem {
  SettingsDisplayLanguage,
#if PLATFORM_ASTERIX
  SettingsDisplayOrientation,
#endif
  SettingsDisplayBacklightMode,
  SettingsDisplayMotionSensor,
  SettingsDisplayAmbientSensor,
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  SettingsDisplayDynamicIntensity,
#endif
  SettingsDisplayBacklightIntensity,
  SettingsDisplayBacklightTimeout,
#if PLATFORM_SPALDING
  SettingsDisplayAdjustAlignment,
#endif
#if PLATFORM_OBELIX
  SettingsDisplayLegacyAppMode,
#endif
  NumSettingsDisplayItems
};

// number of items under SettingsDisplayBacklightMode which are hidden when backlight is disabled
static const int NUM_BACKLIGHT_SUB_ITEMS = CLIP(SettingsDisplayBacklightTimeout -
                                           SettingsDisplayBacklightMode - 1, 0, NumSettingsDisplayItems);

static bool prv_should_show_backlight_sub_items() {
  return backlight_is_enabled();
}

uint16_t prv_get_item_from_row(uint16_t row) {  
  if (!prv_should_show_backlight_sub_items() && (row > SettingsDisplayBacklightMode)) {
    row += NUM_BACKLIGHT_SUB_ITEMS;
  }

  return row;
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  switch (prv_get_item_from_row(row)) {
    case SettingsDisplayLanguage:
      shell_prefs_toggle_language_english();
      break;
#if PLATFORM_ASTERIX
    case SettingsDisplayOrientation:
      prv_display_orientation_menu_push(data);
      break;
#endif
    case SettingsDisplayBacklightMode:
      light_toggle_enabled();
      break;
    case SettingsDisplayMotionSensor:
      backlight_set_motion_enabled(!backlight_is_motion_enabled());
      break;
    case SettingsDisplayAmbientSensor:
      light_toggle_ambient_sensor_enabled();
      break;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
    case SettingsDisplayDynamicIntensity:
      backlight_set_dynamic_intensity_enabled(!backlight_is_dynamic_intensity_enabled());
      break;
#endif
    case SettingsDisplayBacklightIntensity:
      prv_intensity_menu_push(data);
      break;
    case SettingsDisplayBacklightTimeout:
      prv_timeout_menu_push(data);
      break;
#if PLATFORM_SPALDING
    case SettingsDisplayAdjustAlignment:
      settings_display_calibration_push(app_state_get_window_stack());
      break;
#endif
#if PLATFORM_OBELIX
    case SettingsDisplayLegacyAppMode:
      prv_legacy_app_mode_menu_push(data);
      break;
#endif
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemDisplay);
  settings_menu_mark_dirty(SettingsMenuItemDisplay);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsDisplayData *data = (SettingsDisplayData*) context;
  
  const char *title = NULL;
  const char *subtitle = NULL;
  switch (prv_get_item_from_row(row)) {
    case SettingsDisplayLanguage:
      title = i18n_noop("Language");
      subtitle = i18n_get_lang_name();
      break;
#if PLATFORM_ASTERIX
    case SettingsDisplayOrientation:
      title = i18n_noop("Orientation");
      subtitle = s_display_orientation_labels[prv_display_orientation_get_selection_index()];
      break;
#endif
    case SettingsDisplayBacklightMode:
      title = i18n_noop("Backlight");
      if (backlight_is_enabled()) {
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
        // Show current backlight percentage when dynamic backlight is enabled
        if (backlight_is_dynamic_intensity_enabled()) {
          uint8_t current_percent = light_get_current_brightness_percent();
          snprintf(data->backlight_percent_buffer, sizeof(data->backlight_percent_buffer),
                   "On - %"PRIu8"%%", current_percent);
          subtitle = data->backlight_percent_buffer;
        } else {
          subtitle = i18n_noop("On");
        }
#else
        subtitle = i18n_noop("On");
#endif
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
    case SettingsDisplayMotionSensor:
      title = i18n_noop("Motion Enabled");
      if (backlight_is_motion_enabled()) {
        subtitle = i18n_noop("On");
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
    case SettingsDisplayAmbientSensor:
      title = i18n_noop("Ambient Sensor");
      if (backlight_is_ambient_sensor_enabled()) {
        // Display ALS value when ambient sensor is enabled
        uint32_t als_value = ambient_light_get_light_level();
        snprintf(data->als_value_buffer, sizeof(data->als_value_buffer), 
                 "On (%"PRIu32")", als_value);
        subtitle = data->als_value_buffer;
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
    case SettingsDisplayDynamicIntensity:
      title = i18n_noop("Dynamic Backlight");
      if (backlight_is_dynamic_intensity_enabled()) {
        subtitle = i18n_noop("On");
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
#endif
    case SettingsDisplayBacklightIntensity:
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
      if (backlight_is_dynamic_intensity_enabled()) {
        title = i18n_noop("Max Intensity");
      } else {
        title = i18n_noop("Intensity");
      }
#else
      title = i18n_noop("Intensity");
#endif
      subtitle = s_intensity_labels[prv_intensity_get_selection_index()];
      break;
    case SettingsDisplayBacklightTimeout:
      title = i18n_noop("Timeout");
      subtitle = s_timeout_labels[prv_timeout_get_selection_index()];
      break;
#if PLATFORM_SPALDING
    case SettingsDisplayAdjustAlignment:
      title = i18n_noop("Screen Alignment");
      break;
#endif
#if PLATFORM_OBELIX
    case SettingsDisplayLegacyAppMode:
      title = i18n_noop("Legacy Apps");
      subtitle = (shell_prefs_get_legacy_app_render_mode() == LegacyAppRenderMode_Scaling) ?
                 i18n_noop("Scaled") : i18n_noop("Centered");
      break;
#endif
    default:
      WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  uint16_t rows = NumSettingsDisplayItems;
  
  if (!prv_should_show_backlight_sub_items()) {
    rows -= NUM_BACKLIGHT_SUB_ITEMS;
  }
  
  return rows;
}

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsDisplayData *data = (SettingsDisplayData*) context;
  if (data->update_timer) {
    app_timer_cancel(data->update_timer);
    data->update_timer = NULL;
  }
  i18n_free_all(data);
  app_free(data);
}

// Timer callback to update debug values
#define UPDATE_INTERVAL_MS 500  // Update twice per second
static void prv_update_timer_cb(void *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  
  // Mark the menu as dirty to trigger a redraw
  settings_menu_mark_dirty(SettingsMenuItemDisplay);
  
  // Reschedule the timer
  data->update_timer = app_timer_register(UPDATE_INTERVAL_MS, prv_update_timer_cb, data);
}

static void prv_appear_cb(SettingsCallbacks *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  if (!data->update_timer) {
    data->update_timer = app_timer_register(UPDATE_INTERVAL_MS, prv_update_timer_cb, data);
  }
}

static void prv_hide_cb(SettingsCallbacks *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  if (data->update_timer) {
    app_timer_cancel(data->update_timer);
    data->update_timer = NULL;
  }
}

static Window *prv_init(void) {
  SettingsDisplayData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsDisplayData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear_cb,
    .hide = prv_hide_cb,
  };

  return settings_window_create(SettingsMenuItemDisplay, &data->callbacks);
}

const SettingsModuleMetadata *settings_display_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Display"),
    .init = prv_init,
  };

  return &s_module_info;
}
