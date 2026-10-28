/* SPDX-License-Identifier: Apache-2.0 */

#include "health.h"

#include "menu.h"
#include "option_menu.h"
#include "window.h"

#include <pbl/kernel/fatal.h>
#include <pbl/services/activity/activity.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/size.h>

#include <applib/ui/option_menu_window.h>
#include <kernel/pbl_malloc.h>
#include <process_state/app_state/app_state.h>
#include <shell/prefs.h>

typedef struct SettingsHealthData {
  SettingsCallbacks callbacks;
} SettingsHealthData;

static const char *s_units_distance_labels[] = {
  i18n_noop("Kilometers"),
  i18n_noop("Miles"),
};

#ifdef CONFIG_SERVICE_HRM
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
#ifdef CONFIG_SERVICE_HRM
  SettingsHealthHRMonitoringInterval,
  SettingsHealthHRActivityTracking,
  SettingsHealthSpO2Enabled,
  SettingsHealthSpO2MonitoringInterval,
  SettingsHealthSpO2ActivityTracking,
#endif
  NumSettingsHealthItems
};

#ifdef CONFIG_SERVICE_HRM
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
  const char *title = i18n_noop("SpO2 Monitoring");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_spo2_interval_labels), true /* icons_enabled */,
                            s_spo2_interval_labels, data);
}
#endif

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

  const char *title = nullptr;
  const char *subtitle = nullptr;

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
#ifdef CONFIG_SERVICE_HRM
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
    case SettingsHealthSpO2Enabled: {
      title = i18n_noop("SpO2");
      subtitle = activity_prefs_spo2_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthSpO2MonitoringInterval: {
      title = i18n_noop("SpO2 Monitoring");
      HRMonitoringInterval interval = activity_prefs_get_spo2_measurement_interval();
      int idx = prv_spo2_interval_to_index(interval);
      subtitle = s_spo2_interval_labels[idx];
      break;
    }
    case SettingsHealthSpO2ActivityTracking: {
      title = i18n_noop("SpO2 During Activity");
      if (!activity_prefs_hrm_activity_tracking_is_enabled()) {
        // Requires HR During Activity to be on first.
        subtitle = i18n_noop("Needs HR During Activity");
      } else {
        subtitle =
            activity_prefs_spo2_activity_tracking_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      }
      break;
    }
#endif
    default:
      pbl_panic();
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), nullptr);
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
#ifdef CONFIG_SERVICE_HRM
    case SettingsHealthHRMonitoringInterval:
      prv_hrm_interval_menu_push((SettingsHealthData *)context);
      break;
    case SettingsHealthHRActivityTracking: {
      bool new_value = !activity_prefs_hrm_activity_tracking_is_enabled();
      activity_prefs_set_hrm_activity_tracking_enabled(new_value);
      if (!new_value) {
        // SpO2 in activity depends on HR in activity; clear it when the parent goes off.
        activity_prefs_set_spo2_activity_tracking_enabled(false);
      }
      break;
    }
    case SettingsHealthSpO2Enabled:
      activity_prefs_set_spo2_enabled(!activity_prefs_spo2_is_enabled());
      break;
    case SettingsHealthSpO2MonitoringInterval:
      prv_spo2_interval_menu_push((SettingsHealthData *)context);
      break;
    case SettingsHealthSpO2ActivityTracking:
      // Only togglable once HR During Activity is on (see draw); otherwise ignore the press.
      if (activity_prefs_hrm_activity_tracking_is_enabled()) {
        activity_prefs_set_spo2_activity_tracking_enabled(
            !activity_prefs_spo2_activity_tracking_is_enabled());
      }
      break;
#endif
    default:
      pbl_panic();
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