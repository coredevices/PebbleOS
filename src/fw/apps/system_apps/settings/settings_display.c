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
