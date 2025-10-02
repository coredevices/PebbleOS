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
  char als_threshold_buffer[16];  // Buffer for formatted ALS threshold
  char als_status_buffer[64];     // Buffer for NumberWindow label with status
  bool als_adjustment_active;     // Track if ALS adjustment is active
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

// ALS Threshold Settings
/////////////////////////////

static void prv_update_als_threshold_label(NumberWindow *number_window, SettingsDisplayData *data) {
  uint32_t current_reading = ambient_light_get_light_level();
  uint32_t current_threshold = (uint32_t)number_window_get_value(number_window);
  bool would_backlight_be_on = current_reading <= current_threshold;
  
  snprintf(data->als_status_buffer, sizeof(data->als_status_buffer), 
           "Backlight: %s",
           would_backlight_be_on ? "ON" : "OFF");
  
  number_window_set_label(number_window, data->als_status_buffer);
}

static void prv_als_threshold_incremented(NumberWindow *number_window, void *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  prv_update_als_threshold_label(number_window, data);
}

static void prv_als_threshold_decremented(NumberWindow *number_window, void *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  prv_update_als_threshold_label(number_window, data);
}

static void prv_als_threshold_selected(NumberWindow *number_window, void *context) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  uint32_t new_threshold = (uint32_t)number_window_get_value(number_window);
  backlight_set_ambient_threshold(new_threshold);
  data->als_adjustment_active = false;
  light_allow(true);
  app_window_stack_remove(&number_window->window, true /* animated */);
}

static void prv_als_threshold_menu_push(SettingsDisplayData *data) {
  // Disable backlight while adjusting ALS threshold to prevent skewing readings
  light_allow(false);
  data->als_adjustment_active = true;

  // Give time for backlight to turn off
  // If we don't do this, the text may say the false result
  // until the user changes the value
  psleep(200);
  
  // Get current ambient light reading to show backlight status
  uint32_t current_reading = ambient_light_get_light_level();
  uint32_t current_threshold = backlight_get_ambient_threshold();
  bool would_backlight_be_on = current_reading <= current_threshold;
  
  // Create descriptive label with current status
  snprintf(data->als_status_buffer, sizeof(data->als_status_buffer), 
           "Backlight: %s",
           would_backlight_be_on ? "ON" : "OFF");
  
  NumberWindow *number_window = number_window_create(
    data->als_status_buffer,
    (NumberWindowCallbacks) {
      .selected = prv_als_threshold_selected,
      .incremented = prv_als_threshold_incremented,
      .decremented = prv_als_threshold_decremented,
    },
    data
  );
  
  if (!number_window) {
    // Re-enable backlight if NumberWindow creation failed
    data->als_adjustment_active = false;
    light_allow(true);
    return;
  }
  
  
  // Set reasonable min/max values for ALS threshold
  number_window_set_min(number_window, 0);
  number_window_set_max(number_window, AMBIENT_LIGHT_LEVEL_MAX);
  number_window_set_step_size(number_window, 1);
  number_window_set_value(number_window, (int32_t)current_threshold);
  
  const bool animated = true;
  app_window_stack_push(&number_window->window, animated);
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
  SettingsDisplayBacklightMode,
  SettingsDisplayMotionSensor,
  SettingsDisplayAmbientSensor,
  SettingsDisplayAmbientThreshold,
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
                                           SettingsDisplayBacklightMode, 0, NumSettingsDisplayItems);

static bool prv_should_show_backlight_sub_items() {
  return backlight_is_enabled();
}

static bool prv_should_show_als_threshold() {
  return backlight_is_enabled() && backlight_is_ambient_sensor_enabled();
}

uint16_t prv_get_item_from_row(uint16_t row) {  
  if (!prv_should_show_backlight_sub_items() && (row > SettingsDisplayBacklightMode)) {
    row += NUM_BACKLIGHT_SUB_ITEMS;
  }

  if (!prv_should_show_als_threshold() && (row >= SettingsDisplayAmbientThreshold)) {
    row++;
  }

  return row;
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsDisplayData *data = (SettingsDisplayData*)context;
  switch (prv_get_item_from_row(row)) {
    case SettingsDisplayLanguage:
      shell_prefs_toggle_language_english();
      break;
    case SettingsDisplayBacklightMode:
      light_toggle_enabled();
      break;
    case SettingsDisplayMotionSensor:
      backlight_set_motion_enabled(!backlight_is_motion_enabled());
      break;
    case SettingsDisplayAmbientSensor:
      light_toggle_ambient_sensor_enabled();
      break;
    case SettingsDisplayAmbientThreshold:
      prv_als_threshold_menu_push(data);
      break;
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
  
  // Check if user canceled out of ALS adjustment (this gets called when we return to settings menu)
  if (data->als_adjustment_active) {
    data->als_adjustment_active = false;
    light_allow(true);
  }
  
  const char *title = NULL;
  const char *subtitle = NULL;
  switch (prv_get_item_from_row(row)) {
    case SettingsDisplayLanguage:
      title = i18n_noop("Language");
      subtitle = i18n_get_lang_name();
      break;
    case SettingsDisplayBacklightMode:
      title = i18n_noop("Backlight");
      if (backlight_is_enabled()) {
        subtitle = i18n_noop("On");
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
        subtitle = i18n_noop("On");
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
    case SettingsDisplayAmbientThreshold: {
      title = i18n_noop("ALS Threshold");
      // Show current threshold value
      uint32_t current_threshold = backlight_get_ambient_threshold();
      snprintf(data->als_threshold_buffer, sizeof(data->als_threshold_buffer), 
               "%"PRIu32, current_threshold);
      subtitle = data->als_threshold_buffer;
      break;
    }
    case SettingsDisplayBacklightIntensity:
      title = i18n_noop("Intensity");
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
  
  if (!prv_should_show_als_threshold()) {
    rows--;
  }
  
  return rows;
}

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsDisplayData *data = (SettingsDisplayData*) context;
  i18n_free_all(data);
  app_free(data);
}

static Window *prv_init(void) {
  SettingsDisplayData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsDisplayData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
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
