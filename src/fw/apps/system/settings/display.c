/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display.h"
#include "display_calibration.h"
#include "menu.h"
#include "option_menu.h"
#include "window.h"

#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "drivers/ambient_light.h"
#include "drivers/battery.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "popups/notifications/notification_window.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/common/i18n/i18n.h"
#include "pbl/services/common/light.h"
#include "shell/prefs.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// Forward decl so the parent menu can push the Backlight submenu.
static void prv_backlight_submenu_push(void);

typedef struct SettingsDisplayData {
  SettingsCallbacks callbacks;
} SettingsDisplayData;

typedef struct SettingsBacklightData {
  SettingsCallbacks callbacks;
  char als_value_buffer[16];  // Buffer for ALS value display
  char backlight_percent_buffer[16];  // Buffer for backlight percentage display
  AppTimer *update_timer;  // Timer for live updating debug values
} SettingsBacklightData;

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

static void prv_intensity_menu_push(SettingsBacklightData *data) {
  const int index = prv_intensity_get_selection_index();
  const OptionMenuCallbacks callbacks = {
    .select = prv_intensity_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("INTENSITY"), i18n_noop("Intensity"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks, ARRAY_LENGTH(s_intensity_labels),
      true /* icons_enabled */, s_intensity_labels, data);
}

#if CAPABILITY_HAS_ORIENTATION_MANAGER
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

static void prv_timeout_menu_push(SettingsBacklightData *data) {
  int index = prv_timeout_get_selection_index();
  const OptionMenuCallbacks callbacks = {
    .select = prv_timeout_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("TIMEOUT"), i18n_noop("Timeout"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks, ARRAY_LENGTH(s_timeout_labels),
      true /* icons_enabled */, s_timeout_labels, data);
}

// Touch Wake Settings
/////////////////////////////
#ifdef CONFIG_TOUCH
static const char *s_touch_wake_labels[] = {
    [BacklightTouchWake_Off] = i18n_noop("Off"),
    [BacklightTouchWake_DoubleTap] = i18n_noop("Double Tap"),
    [BacklightTouchWake_Tap] = i18n_noop("Tap"),
};

static void prv_touch_wake_menu_select(OptionMenu *option_menu, int selection, void *context) {
  backlight_set_touch_wake((BacklightTouchWake)selection);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_touch_wake_menu_push(SettingsBacklightData *data) {
  const int index = (int)backlight_get_touch_wake();
  const OptionMenuCallbacks callbacks = {
    .select = prv_touch_wake_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("WAKE ON TOUCH"), i18n_noop("Wake on touch"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks,
      ARRAY_LENGTH(s_touch_wake_labels), true /* icons_enabled */, s_touch_wake_labels, data);
}
#endif

// Legacy App Mode Settings (Obelix only)
/////////////////////////////
#if CAPABILITY_HAS_APP_SCALING
static const char *s_legacy_app_mode_labels[] = {
    i18n_noop("Centered"),
    i18n_noop("Scaled (Nearest)"),
    i18n_noop("Scaled (Bilinear)")
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

// Backlight submenu
//////////////////////////////////////////////////////////////////////////////

enum SettingsBacklightItem {
  SettingsBacklightMode,
  SettingsBacklightMotionWake,
#ifdef CONFIG_TOUCH
  SettingsBacklightTouchWake,
#endif
  SettingsBacklightAmbientSensor,
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  SettingsBacklightDynamicIntensity,
#endif
  SettingsBacklightIntensity,
  SettingsBacklightTimeout,
  NumSettingsBacklightItems
};

// Number of items after the Mode row that are hidden when the backlight is off.
static const int NUM_BACKLIGHT_SUB_ITEMS = CLIP(SettingsBacklightTimeout -
                                           SettingsBacklightMode - 1, 0, NumSettingsBacklightItems);

static bool prv_should_show_backlight_sub_items(void) {
  return backlight_is_enabled();
}

#ifdef CONFIG_TOUCH
// The wake-on-touch row is only relevant when global touch is enabled.
// It gets hidden dynamically (not just gated at compile time) so users don't
// see a dangling backlight option that can't do anything.
static bool prv_should_show_touch_wake(void) {
  return touch_is_globally_enabled();
}
#endif

static uint16_t prv_backlight_item_from_row(uint16_t row) {
  if (!prv_should_show_backlight_sub_items() && (row > SettingsBacklightMode)) {
    row += NUM_BACKLIGHT_SUB_ITEMS;
#ifdef CONFIG_TOUCH
  } else if (!prv_should_show_touch_wake() && (row >= SettingsBacklightTouchWake)) {
    row += 1;
#endif
  }
  return row;
}

static void prv_backlight_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsBacklightData *data = (SettingsBacklightData*)context;
  switch (prv_backlight_item_from_row(row)) {
    case SettingsBacklightMode:
      light_toggle_enabled();
      break;
    case SettingsBacklightMotionWake:
      backlight_set_motion_enabled(!backlight_is_motion_enabled());
      break;
#ifdef CONFIG_TOUCH
    case SettingsBacklightTouchWake:
      prv_touch_wake_menu_push(data);
      break;
#endif
    case SettingsBacklightAmbientSensor:
      light_toggle_ambient_sensor_enabled();
      break;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
    case SettingsBacklightDynamicIntensity:
      light_toggle_dynamic_intensity_enabled();
      break;
#endif
    case SettingsBacklightIntensity:
      prv_intensity_menu_push(data);
      break;
    case SettingsBacklightTimeout:
      prv_timeout_menu_push(data);
      break;
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemDisplay);
  settings_menu_mark_dirty(SettingsMenuItemDisplay);
}

static void prv_backlight_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                      const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsBacklightData *data = (SettingsBacklightData*) context;
  const char *title = NULL;
  const char *subtitle = NULL;
  switch (prv_backlight_item_from_row(row)) {
    case SettingsBacklightMode:
      title = i18n_noop("Backlight");
      if (backlight_is_enabled()) {
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
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
    case SettingsBacklightMotionWake:
      title = i18n_noop("Wake on motion");
      subtitle = backlight_is_motion_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
#ifdef CONFIG_TOUCH
    case SettingsBacklightTouchWake:
      title = i18n_noop("Wake on touch");
      subtitle = s_touch_wake_labels[backlight_get_touch_wake()];
      break;
#endif
    case SettingsBacklightAmbientSensor:
      title = i18n_noop("Ambient Sensor");
      if (backlight_is_ambient_sensor_enabled()) {
        uint32_t als_value = ambient_light_get_light_level();
        snprintf(data->als_value_buffer, sizeof(data->als_value_buffer),
                 "On (%"PRIu32")", als_value);
        subtitle = data->als_value_buffer;
      } else {
        subtitle = i18n_noop("Off");
      }
      break;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
    case SettingsBacklightDynamicIntensity:
      title = i18n_noop("Dynamic Backlight");
      subtitle = backlight_is_dynamic_intensity_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
#endif
    case SettingsBacklightIntensity:
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
      title = backlight_is_dynamic_intensity_enabled() ? i18n_noop("Max Intensity")
                                                       : i18n_noop("Intensity");
#else
      title = i18n_noop("Intensity");
#endif
      subtitle = s_intensity_labels[prv_intensity_get_selection_index()];
      break;
    case SettingsBacklightTimeout:
      title = i18n_noop("Timeout");
      subtitle = s_timeout_labels[prv_timeout_get_selection_index()];
      break;
    default:
      WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static uint16_t prv_backlight_num_rows_cb(SettingsCallbacks *context) {
  uint16_t rows = NumSettingsBacklightItems;
  if (!prv_should_show_backlight_sub_items()) {
    rows -= NUM_BACKLIGHT_SUB_ITEMS;
#ifdef CONFIG_TOUCH
  } else if (!prv_should_show_touch_wake()) {
    // Only deduct when the sub-items are otherwise visible; when the backlight
    // itself is off the touch-wake row is already covered by the bulk collapse.
    rows -= 1;
#endif
  }
  return rows;
}

// Timer refresh while the Backlight submenu is visible — lets us live-update
// the ALS reading and the dynamic-backlight percentage subtitle.
#define UPDATE_INTERVAL_MS 500
static void prv_backlight_update_timer_cb(void *context) {
  SettingsBacklightData *data = (SettingsBacklightData*)context;
  settings_menu_mark_dirty(SettingsMenuItemDisplay);
  data->update_timer = app_timer_register(UPDATE_INTERVAL_MS, prv_backlight_update_timer_cb, data);
}

static void prv_backlight_appear_cb(SettingsCallbacks *context) {
  SettingsBacklightData *data = (SettingsBacklightData*)context;
  if (!data->update_timer) {
    data->update_timer = app_timer_register(UPDATE_INTERVAL_MS,
                                            prv_backlight_update_timer_cb, data);
  }
}

static void prv_backlight_hide_cb(SettingsCallbacks *context) {
  SettingsBacklightData *data = (SettingsBacklightData*)context;
  if (data->update_timer) {
    app_timer_cancel(data->update_timer);
    data->update_timer = NULL;
  }
}

static void prv_backlight_deinit_cb(SettingsCallbacks *context) {
  SettingsBacklightData *data = (SettingsBacklightData*) context;
  if (data->update_timer) {
    app_timer_cancel(data->update_timer);
    data->update_timer = NULL;
  }
  i18n_free_all(data);
  app_free(data);
}

static void prv_backlight_submenu_push(void) {
  SettingsBacklightData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsBacklightData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_backlight_deinit_cb,
    .draw_row = prv_backlight_draw_row_cb,
    .select_click = prv_backlight_select_click_cb,
    .num_rows = prv_backlight_num_rows_cb,
    .appear = prv_backlight_appear_cb,
    .hide = prv_backlight_hide_cb,
  };

  Window *window = settings_window_create_with_title(SettingsMenuItemDisplay,
                                                     i18n_noop("Backlight"), &data->callbacks);
  app_window_stack_push(window, true /* animated */);
}

// Display top-level menu
//////////////////////////////////////////////////////////////////////////////

enum SettingsDisplayItem {
  SettingsDisplayLanguage,
#if CAPABILITY_HAS_ORIENTATION_MANAGER
  SettingsDisplayOrientation,
#endif
#ifdef CONFIG_TOUCH
  SettingsDisplayTouch,
#endif
  SettingsDisplayBacklight,
#if PLATFORM_SPALDING && !PLATFORM_SPALDING_GABBRO
  SettingsDisplayAdjustAlignment,
#endif
#if CAPABILITY_HAS_APP_SCALING
  SettingsDisplayLegacyAppMode,
#endif
  NumSettingsDisplayItems
};

static void prv_display_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case SettingsDisplayLanguage:
      shell_prefs_toggle_language_english();
      break;
#if CAPABILITY_HAS_ORIENTATION_MANAGER
    case SettingsDisplayOrientation:
      prv_display_orientation_menu_push((SettingsDisplayData*)context);
      break;
#endif
#ifdef CONFIG_TOUCH
    case SettingsDisplayTouch:
      touch_set_globally_enabled(!touch_is_globally_enabled());
      break;
#endif
    case SettingsDisplayBacklight:
      prv_backlight_submenu_push();
      break;
#if PLATFORM_SPALDING && !PLATFORM_SPALDING_GABBRO
    case SettingsDisplayAdjustAlignment:
      settings_display_calibration_push(app_state_get_window_stack());
      break;
#endif
#if CAPABILITY_HAS_APP_SCALING
    case SettingsDisplayLegacyAppMode:
      prv_legacy_app_mode_menu_push((SettingsDisplayData*)context);
      break;
#endif
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemDisplay);
  settings_menu_mark_dirty(SettingsMenuItemDisplay);
}

static void prv_display_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                    const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsDisplayData *data = (SettingsDisplayData*) context;
  const char *title = NULL;
  const char *subtitle = NULL;
  switch (row) {
    case SettingsDisplayLanguage:
      title = i18n_noop("Language");
      subtitle = i18n_get_lang_name();
      break;
#if CAPABILITY_HAS_ORIENTATION_MANAGER
    case SettingsDisplayOrientation:
      title = i18n_noop("Orientation");
      subtitle = s_display_orientation_labels[prv_display_orientation_get_selection_index()];
      break;
#endif
#ifdef CONFIG_TOUCH
    case SettingsDisplayTouch:
      title = i18n_noop("Touch");
      subtitle = touch_is_globally_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
#endif
    case SettingsDisplayBacklight:
      title = i18n_noop("Backlight");
      break;
#if PLATFORM_SPALDING && !PLATFORM_SPALDING_GABBRO
    case SettingsDisplayAdjustAlignment:
      title = i18n_noop("Screen Alignment");
      break;
#endif
#if CAPABILITY_HAS_APP_SCALING
    case SettingsDisplayLegacyAppMode:
      title = i18n_noop("Legacy Apps");
      subtitle = (shell_prefs_get_legacy_app_render_mode() >= LegacyAppRenderMode_ScalingNearest) ?
                 i18n_noop("Scaled") : i18n_noop("Centered");
      break;
#endif
    default:
      WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static uint16_t prv_display_num_rows_cb(SettingsCallbacks *context) {
  return NumSettingsDisplayItems;
}

static void prv_display_deinit_cb(SettingsCallbacks *context) {
  SettingsDisplayData *data = (SettingsDisplayData*) context;
  i18n_free_all(data);
  app_free(data);
}

static Window *prv_init(void) {
  SettingsDisplayData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsDisplayData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_display_deinit_cb,
    .draw_row = prv_display_draw_row_cb,
    .select_click = prv_display_select_click_cb,
    .num_rows = prv_display_num_rows_cb,
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
