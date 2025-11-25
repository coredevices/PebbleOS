/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_vibe_patterns.h"
#include "settings_window.h"

#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "services/common/analytics/analytics_event.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/notifications/alerts_preferences_private.h"
#include "services/normal/vibes/vibe_client.h"
#include "services/normal/vibes/vibe_intensity.h"
#include "services/normal/vibes/vibe_score.h"
#include "services/normal/vibes/vibe_score_info.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/string.h"

#include <string.h>

typedef enum VibeSettingsRow {
  VibeSettingsRow_Notifications = 0,
  VibeSettingsRow_PhoneCalls,
  VibeSettingsRow_Alarms,
  VibeSettingsRow_System,
  VibeSettingsRow_Count,
} VibeSettingsRow;

typedef struct SettingsVibePatternsData {
  SettingsCallbacks callbacks;
  unsigned int toggled_vibes_mask;
} SettingsVibePatternsData;

static void prv_log_analytic_if_toggled(VibePatternFeature feature, VibeClient client,
                                        SettingsVibePatternsData *data) {
  if ((data->toggled_vibes_mask & feature) == feature) {
    analytics_event_vibe_access(feature, alerts_preferences_get_vibe_score_for_client(client));
  }
}

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;
  i18n_free_all(data);
  prv_log_analytic_if_toggled(VibePatternFeature_Notifications, VibeClient_Notifications, data);
  prv_log_analytic_if_toggled(VibePatternFeature_PhoneCalls, VibeClient_PhoneCalls, data);
  prv_log_analytic_if_toggled(VibePatternFeature_Alarms, VibeClient_Alarms, data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;

  const char *title = NULL;
  const char *subtitle = NULL;

  VibeClient client = VibeClient_Notifications;
  switch (row) {
    case VibeSettingsRow_Notifications: {
      title = i18n_noop("Notifications");
      client = VibeClient_Notifications;
      break;
    }
    case VibeSettingsRow_PhoneCalls: {
      title = i18n_noop("Incoming Calls");
      client = VibeClient_PhoneCalls;
      break;
    }
    case VibeSettingsRow_Alarms: {
      title = i18n_noop("Alarms");
      client = VibeClient_Alarms;
      break;
    }
    case VibeSettingsRow_System: {
      /// Refers to the class of all non-score vibes, e.g. 3rd party app vibes
      title = i18n_noop("System");

      const VibeIntensity current_system_default_vibe_intensity = vibe_intensity_get();
      subtitle = vibe_intensity_get_string_for_intensity(current_system_default_vibe_intensity);
      break;
    }
    default: {
      WTF;
    }
  }

  // We need to set the subtitle to the name of a vibe score if it's NULL at this point
  if (!subtitle) {
    subtitle = vibe_score_info_get_name(alerts_preferences_get_vibe_score_for_client(client));
    if (subtitle && IS_EMPTY_STRING(subtitle)) {
      subtitle = NULL;
    }
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_selection_changed_cb(SettingsCallbacks *context, uint16_t new_row,
                                     uint16_t old_row) {
  vibes_cancel();
  VibeScore *score;
  switch (new_row) {
    case VibeSettingsRow_Notifications: {
      score = vibe_client_get_score(VibeClient_Notifications);
      break;
    }
    case VibeSettingsRow_PhoneCalls: {
      score = vibe_client_get_score(VibeClient_PhoneCalls);
      break;
    }
    case VibeSettingsRow_Alarms: {
      score = vibe_client_get_score(VibeClient_Alarms);
      break;
    }
    case VibeSettingsRow_System: {
      // Vibe a short pulse so the user can feel the current system default vibe intensity
      vibes_short_pulse();
      // Just return because the remainder of this function only applies to vibe scores
      return;
    }
    default:
      WTF;
  }
  if (!score) {
    PBL_LOG(LOG_LEVEL_ERROR, "Null VibeScore!");
    return;
  }
  vibe_score_do_vibe(score);
  vibe_score_destroy(score);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  vibes_cancel();

  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;
  VibeClient client;
  switch (row) {
    case VibeSettingsRow_Notifications: {
      data->toggled_vibes_mask |= VibePatternFeature_Notifications;
      client = VibeClient_Notifications;
      break;
    }
    case VibeSettingsRow_PhoneCalls: {
      data->toggled_vibes_mask |= VibePatternFeature_PhoneCalls;
      client = VibeClient_PhoneCalls;
      break;
    }
    case VibeSettingsRow_Alarms: {
      data->toggled_vibes_mask |= VibePatternFeature_Alarms;
      client = VibeClient_Alarms;
      break;
    }
    case VibeSettingsRow_System: {
      const VibeIntensity current_system_default_vibe_intensity = vibe_intensity_get();
      const VibeIntensity next_system_default_vibe_intensity =
        vibe_intensity_cycle_next(current_system_default_vibe_intensity);

      // Set the next system default vibe intensity and vibe a short pulse so the user can feel it
      vibe_intensity_set(next_system_default_vibe_intensity);
      alerts_preferences_set_vibe_intensity(next_system_default_vibe_intensity);
      vibes_short_pulse();

      settings_menu_mark_dirty(SettingsMenuItemVibrations);

      // Just return because the remainder of this function only applies to vibe scores
      return;
    }
    default:
      WTF;
  }

  VibeScoreId current_vibe_score = alerts_preferences_get_vibe_score_for_client(client);
  VibeScoreId new_vibe_score = vibe_score_info_cycle_next(client, current_vibe_score);
  alerts_preferences_set_vibe_score_for_client(client, new_vibe_score);
  settings_menu_mark_dirty(SettingsMenuItemVibrations);
  VibeScore *score = vibe_client_get_score(client);
  if (!score) {
    return;
  }
  vibe_score_do_vibe(score);
  vibe_score_destroy(score);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return VibeSettingsRow_Count;
}

static void prv_expand_cb(SettingsCallbacks *context) {
  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;

  // window is visible again, remind user which vibe pattern they're on
  int16_t current_row = settings_menu_get_selected_row(SettingsMenuItemVibrations);
  prv_selection_changed_cb(&data->callbacks, current_row, 0);

  settings_menu_mark_dirty(SettingsMenuItemVibrations);
}

static void prv_hide_cb(SettingsCallbacks *context) {
  vibes_cancel();
}

static Window *prv_init(void) {
  SettingsVibePatternsData *data = app_zalloc_check(sizeof(SettingsVibePatternsData));

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .selection_changed = prv_selection_changed_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .expand = prv_expand_cb,
    .hide = prv_hide_cb,
  };

  return settings_window_create(SettingsMenuItemVibrations, &data->callbacks);
}

const SettingsModuleMetadata *settings_vibe_patterns_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Vibrations"),
    .init = prv_init,
  };

  return &s_module_info;
}
