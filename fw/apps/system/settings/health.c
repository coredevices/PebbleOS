/* SPDX-License-Identifier: Apache-2.0 */

#include "health.h"
#include "menu.h"
#include "option_menu.h"
#include "window.h"

#include "applib/ui/app_window_stack.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/value_picker_window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/activity/activity.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "pbl/util/size.h"

#include <inttypes.h>
#include <stdio.h>

#define HEIGHT_MIN_CM 100
#define HEIGHT_MAX_CM 250
#define HEIGHT_MIN_IN 39
#define HEIGHT_MAX_IN 98
#define MM_PER_IN_X10 254
#define AGE_MIN_YEARS 10
#define AGE_MAX_YEARS 100

typedef struct SettingsHealthData {
  SettingsCallbacks callbacks;
  char height_subtitle[16];
  char age_subtitle[8];
} SettingsHealthData;

typedef struct SettingsHealthPicker {
  ValuePickerWindow picker_window;
  void (*save)(int32_t value);
} SettingsHealthPicker;

static const char *s_units_distance_labels[] = {
  i18n_noop("Kilometers"),
  i18n_noop("Miles"),
};

#ifdef CONFIG_HRM
static const HRMonitoringInterval s_hrm_intervals[] = {
  HRMonitoringInterval_5Min,  HRMonitoringInterval_10Min,    HRMonitoringInterval_30Min,
  HRMonitoringInterval_1Hour, HRMonitoringInterval_Disabled,
};

static const char *s_hrm_interval_labels[] = {
  i18n_noop("5 Minutes"), i18n_noop("10 Minutes"), i18n_noop("30 Minutes"),
  i18n_noop("1 Hour"),    i18n_noop("Disabled"),
};

static int prv_hrm_interval_to_index(HRMonitoringInterval interval) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_hrm_intervals); i++) {
    if (s_hrm_intervals[i] == interval) {
      return (int)i;
    }
  }
  return prv_hrm_interval_to_index(HRMonitoringInterval_10Min);
}

static const HRMonitoringInterval s_spo2_intervals[] = {
  HRMonitoringInterval_10Min,
  HRMonitoringInterval_30Min,
  HRMonitoringInterval_1Hour,
  HRMonitoringInterval_Disabled,
};

static const char *s_spo2_interval_labels[] = {
  i18n_noop("10 Minutes"),
  i18n_noop("30 Minutes"),
  i18n_noop("1 Hour"),
  i18n_noop("Disabled"),
};

static int prv_spo2_interval_to_index(HRMonitoringInterval interval) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_spo2_intervals); i++) {
    if (s_spo2_intervals[i] == interval) {
      return (int)i;
    }
  }
  return prv_spo2_interval_to_index(HRMonitoringInterval_10Min);
}
#endif

enum SettingsHealthItem {
  SettingsHealthTrackingEnabled,
  SettingsHealthUnitDistance,
  SettingsHealthHeight,
  SettingsHealthAge,
#ifdef CONFIG_HRM
  SettingsHealthHRMonitoringInterval,
  SettingsHealthHRActivityTracking,
  SettingsHealthBloodOxygenEnabled,
  SettingsHealthSpO2MonitoringInterval,
  SettingsHealthBloodOxygenActivityTracking,
#endif
  NumSettingsHealthItems
};

#ifdef CONFIG_HRM
// HRM Interval option menu
/////////////////////////////

static void prv_hrm_interval_menu_select(OptionMenu *option_menu, int selection, void *context) {
  if (selection >= 0 && (size_t)selection < ARRAY_LENGTH(s_hrm_intervals)) {
    activity_prefs_set_hrm_measurement_interval(s_hrm_intervals[selection]);
  }
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_hrm_interval_menu_push(SettingsHealthData *data) {
  const int index = prv_hrm_interval_to_index(activity_prefs_get_hrm_measurement_interval());
  const OptionMenuCallbacks callbacks = {
    .select = prv_hrm_interval_menu_select,
  };
  const char *title = i18n_noop("HR Monitoring");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_hrm_interval_labels), true /* icons_enabled */,
                            s_hrm_interval_labels, data);
}

// SpO2 Interval option menu
/////////////////////////////

static void prv_spo2_interval_menu_select(OptionMenu *option_menu, int selection, void *context) {
  if (selection >= 0 && (size_t)selection < ARRAY_LENGTH(s_spo2_intervals)) {
    activity_prefs_set_spo2_measurement_interval(s_spo2_intervals[selection]);
  }
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_spo2_interval_menu_push(SettingsHealthData *data) {
  const int index = prv_spo2_interval_to_index(activity_prefs_get_spo2_measurement_interval());
  const OptionMenuCallbacks callbacks = {
    .select = prv_spo2_interval_menu_select,
  };
  const char *title = i18n_noop("Blood Oxygen");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_spo2_interval_labels), true /* icons_enabled */,
                            s_spo2_interval_labels, data);
}
#endif

// Height / Age value pickers
/////////////////////////////

static bool prv_height_is_imperial(void) {
  return shell_prefs_get_units_distance() == UnitsDistance_Miles;
}

static int32_t prv_height_value(void) {
  const int32_t height_mm = activity_prefs_get_height_mm();
  return prv_height_is_imperial() ? (height_mm * 10 + MM_PER_IN_X10 / 2) / MM_PER_IN_X10
                                  : (height_mm + 5) / 10;
}

static void prv_save_height(int32_t value) {
  activity_prefs_set_height_mm(prv_height_is_imperial() ? (value * MM_PER_IN_X10 + 5) / 10
                                                        : value * 10);
}

static void prv_save_age(int32_t value) {
  activity_prefs_set_age_years(value);
}

static void prv_picker_selected(ValuePickerWindow *picker_window, void *context) {
  SettingsHealthPicker *picker = context;
  picker->save(value_picker_window_get_value(picker_window));
  settings_menu_reload_data(SettingsMenuItemHealth);
  settings_menu_mark_dirty(SettingsMenuItemHealth);
  app_window_stack_remove(&picker_window->window, true /* animated */);
}

static void prv_picker_unload(ValuePickerWindow *picker_window, void *context) {
  i18n_free_all(context);
  app_free(context);
}

static void prv_picker_push(const char *title, const char *unit, int32_t value, int32_t min_value,
                            int32_t max_value, void (*save)(int32_t value)) {
  SettingsHealthPicker *picker = app_zalloc_check(sizeof(*picker));
  picker->save = save;
  const ValuePickerContent content = {
    .title = i18n_get(title, picker),
    .unit = unit ? i18n_get(unit, picker) : NULL,
    .value = value,
    .min_value = min_value,
    .max_value = max_value,
    .step = 1,
  };
  value_picker_window_init(&picker->picker_window, &content, NULL,
                           (ValuePickerWindowCallbacks){
                             .selected = prv_picker_selected,
                             .unload = prv_picker_unload,
                           },
                           picker);
  app_window_stack_push(&picker->picker_window.window, true /* animated */);
}

static void prv_height_picker_push(void) {
  if (prv_height_is_imperial()) {
    prv_picker_push(i18n_noop("Height"), i18n_noop("in"), prv_height_value(), HEIGHT_MIN_IN,
                    HEIGHT_MAX_IN, prv_save_height);
  } else {
    prv_picker_push(i18n_noop("Height"), i18n_noop("cm"), prv_height_value(), HEIGHT_MIN_CM,
                    HEIGHT_MAX_CM, prv_save_height);
  }
}

static void prv_age_picker_push(void) {
  prv_picker_push(i18n_noop("Age"), NULL, activity_prefs_get_age_years(), AGE_MIN_YEARS,
                  AGE_MAX_YEARS, prv_save_age);
}

// Menu Callbacks
/////////////////////////////

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsHealthData *data = (SettingsHealthData *)context;

  i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx, const Layer *cell_layer,
                            uint16_t row, bool selected) {
  SettingsHealthData *data = (SettingsHealthData *)context;

  const char *title = NULL;
  const char *subtitle = NULL;

  switch (row) {
    case SettingsHealthTrackingEnabled: {
      title = i18n_noop("Health Tracking");
      subtitle = activity_prefs_tracking_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthUnitDistance: {
      title = i18n_noop("Distance Unit");
      UnitsDistance unit = shell_prefs_get_units_distance();
      if (unit >= UnitsDistanceCount) {
        subtitle = i18n_noop("Unknown");
      } else {
        subtitle = s_units_distance_labels[unit];
      }
      break;
    }
    case SettingsHealthHeight: {
      const int32_t height = prv_height_value();
      if (prv_height_is_imperial()) {
        snprintf(data->height_subtitle, sizeof(data->height_subtitle), "%" PRId32 "'%" PRId32 "\"",
                 height / 12, height % 12);
      } else {
        snprintf(data->height_subtitle, sizeof(data->height_subtitle), "%" PRId32 " %s", height,
                 i18n_get("cm", data));
      }
      menu_cell_basic_draw(ctx, cell_layer, i18n_get("Height", data), data->height_subtitle, NULL);
      return;
    }
    case SettingsHealthAge: {
      snprintf(data->age_subtitle, sizeof(data->age_subtitle), "%u",
               (unsigned int)activity_prefs_get_age_years());
      menu_cell_basic_draw(ctx, cell_layer, i18n_get("Age", data), data->age_subtitle, NULL);
      return;
    }
#ifdef CONFIG_HRM
    case SettingsHealthHRMonitoringInterval: {
      title = i18n_noop("HR Monitoring");
      HRMonitoringInterval interval = activity_prefs_get_hrm_measurement_interval();
      int idx = prv_hrm_interval_to_index(interval);
      subtitle = s_hrm_interval_labels[idx];
      break;
    }
    case SettingsHealthHRActivityTracking: {
      title = i18n_noop("HR During Activity");
      subtitle =
          activity_prefs_hrm_activity_tracking_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthBloodOxygenEnabled: {
      title = i18n_noop("Blood Oxygen");
      subtitle = activity_prefs_blood_oxygen_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthSpO2MonitoringInterval: {
      title = i18n_noop("SpO2 Monitoring");
      HRMonitoringInterval interval = activity_prefs_get_spo2_measurement_interval();
      int idx = prv_spo2_interval_to_index(interval);
      subtitle = s_spo2_interval_labels[idx];
      break;
    }
    case SettingsHealthBloodOxygenActivityTracking: {
      title = i18n_noop("Blood O2 During Activity");
      if (!activity_prefs_hrm_activity_tracking_is_enabled()) {
        // Requires HR During Activity to be on first.
        subtitle = i18n_noop("Needs HR During Activity");
      } else {
        subtitle = activity_prefs_blood_oxygen_activity_tracking_is_enabled() ? i18n_noop("On")
                                                                              : i18n_noop("Off");
      }
      break;
    }
#endif
    default:
      WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case SettingsHealthTrackingEnabled: {
      bool new_value = !activity_prefs_tracking_is_enabled();
      activity_prefs_tracking_set_enabled(new_value);
      if (new_value) {
        activity_start_tracking(false);
      } else {
        activity_stop_tracking();
      }
      break;
    }
    case SettingsHealthUnitDistance: {
      UnitsDistance unit = shell_prefs_get_units_distance();
      unit = (unit + 1) % UnitsDistanceCount;
      shell_prefs_set_units_distance(unit);
      break;
    }
    case SettingsHealthHeight:
      prv_height_picker_push();
      return;
    case SettingsHealthAge:
      prv_age_picker_push();
      return;
#ifdef CONFIG_HRM
    case SettingsHealthHRMonitoringInterval:
      prv_hrm_interval_menu_push((SettingsHealthData *)context);
      break;
    case SettingsHealthHRActivityTracking: {
      bool new_value = !activity_prefs_hrm_activity_tracking_is_enabled();
      activity_prefs_set_hrm_activity_tracking_enabled(new_value);
      if (!new_value) {
        // Blood O2 in activity depends on HR in activity; clear it when the parent goes off.
        activity_prefs_set_blood_oxygen_activity_tracking_enabled(false);
      }
      break;
    }
    case SettingsHealthBloodOxygenEnabled:
      activity_prefs_set_blood_oxygen_enabled(!activity_prefs_blood_oxygen_is_enabled());
      break;
    case SettingsHealthSpO2MonitoringInterval:
      prv_spo2_interval_menu_push((SettingsHealthData *)context);
      break;
    case SettingsHealthBloodOxygenActivityTracking:
      // Only togglable once HR During Activity is on (see draw); otherwise ignore the press.
      if (activity_prefs_hrm_activity_tracking_is_enabled()) {
        activity_prefs_set_blood_oxygen_activity_tracking_enabled(
            !activity_prefs_blood_oxygen_activity_tracking_is_enabled());
      }
      break;
#endif
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemHealth);
  settings_menu_mark_dirty(SettingsMenuItemHealth);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  if (!activity_prefs_tracking_is_enabled()) {
    return 1; // Only show the Health Tracking toggle
  }
  return NumSettingsHealthItems;
}

static void prv_appear_cb(SettingsCallbacks *context) {
}

static void prv_hide_cb(SettingsCallbacks *context) {
}

static Window *prv_init(void) {
  SettingsHealthData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsHealthData){};

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear_cb,
    .hide = prv_hide_cb,
  };

  return settings_window_create(SettingsMenuItemHealth, &data->callbacks);
}

const SettingsModuleMetadata *settings_health_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Health"),
    .init = prv_init,
  };

  return &s_module_info;
}