/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "vibe_patterns.h"

#include "option_menu.h"
#include "speaker_volume_window.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/fatal.h>
#include <pbl/logging/logging.h>
#include <pbl/services/notifications/alerts_preferences.h>
#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/speaker/speaker_service.h>
#include <pbl/services/vibes/vibe_client.h>
#include <pbl/services/vibes/vibe_intensity.h>
#include <pbl/services/vibes/vibe_score.h>
#include <pbl/services/vibes/vibe_score_info.h>
#include <pbl/util/string.h>

#include <applib/app_malloc.h>
#include <applib/ui/ui.h>

typedef enum VibeSettingsRow {
#ifdef CONFIG_SPEAKER
  VibeSettingsRow_MuteSpeaker = 0,
  VibeSettingsRow_SpeakerVolume,
  VibeSettingsRow_Notifications,
#else
  VibeSettingsRow_Notifications = 0,
#endif
  VibeSettingsRow_PhoneCalls,
  VibeSettingsRow_Alarms,
  VibeSettingsRow_Hourly,
  VibeSettingsRow_OnDisconnect,
  VibeSettingsRow_System,
  VibeSettingsRow_Count,
} VibeSettingsRow;

typedef struct SettingsVibePatternsData {
  SettingsCallbacks callbacks;
  unsigned int toggled_vibes_mask;
#ifdef CONFIG_SPEAKER
  char volume_subtitle[8]; // "100%" + NUL
#endif
} SettingsVibePatternsData;

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;
  pbl_i18n_free_all(data);
  app_free(data);
}

static const char *prv_row_title(uint16_t row) {
  switch (row) {
    case VibeSettingsRow_Notifications:
      return pbl_i18n_noop("Notifications");
    case VibeSettingsRow_PhoneCalls:
      return pbl_i18n_noop("Incoming Calls");
    case VibeSettingsRow_Alarms:
      return pbl_i18n_noop("Alarms");
    case VibeSettingsRow_Hourly:
      return pbl_i18n_noop("Hourly Notice");
    case VibeSettingsRow_OnDisconnect:
      return pbl_i18n_noop("On Disconnect");
    case VibeSettingsRow_System:
      /// Refers to the class of all non-score vibes, e.g. 3rd party app vibes
      return pbl_i18n_noop("System");
    default:
      pbl_panic();
  }
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx, const Layer *cell_layer,
                            uint16_t row, bool selected) {
  SettingsVibePatternsData *data = (SettingsVibePatternsData *)context;

  const char *title = nullptr;
  const char *subtitle = nullptr;

  VibeClient client = VibeClient_Notifications;
  switch (row) {
#ifdef CONFIG_SPEAKER
    case VibeSettingsRow_MuteSpeaker: {
      title = pbl_i18n_noop("Mute Speaker");
      subtitle =
          alerts_preferences_get_speaker_muted() ? pbl_i18n_noop("On") : pbl_i18n_noop("Off");
      menu_cell_basic_draw(ctx, cell_layer, pbl_i18n_get(title, data), pbl_i18n_get(subtitle, data),
                           nullptr);
      return;
    }
    case VibeSettingsRow_SpeakerVolume: {
      title = pbl_i18n_noop("Volume");
      snprintf(data->volume_subtitle, sizeof(data->volume_subtitle), "%u%%",
               alerts_preferences_get_speaker_volume());
      menu_cell_basic_draw(ctx, cell_layer, pbl_i18n_get(title, data), data->volume_subtitle,
                           nullptr);
      return;
    }
#endif
    case VibeSettingsRow_Notifications: {
      client = VibeClient_Notifications;
      break;
    }
    case VibeSettingsRow_PhoneCalls: {
      client = VibeClient_PhoneCalls;
      break;
    }
    case VibeSettingsRow_Alarms: {
      client = VibeClient_Alarms;
      break;
    }
    case VibeSettingsRow_Hourly: {
      client = VibeClient_Hourly;
      break;
    }
    case VibeSettingsRow_OnDisconnect: {
      client = VibeClient_OnDisconnect;
      break;
    }
    case VibeSettingsRow_System: {
      const VibeIntensity current_system_default_vibe_intensity = vibe_intensity_get();
      subtitle = vibe_intensity_get_string_for_intensity(current_system_default_vibe_intensity);
      break;
    }
    default: {
      pbl_panic();
    }
  }

  title = prv_row_title(row);

  // We need to set the subtitle to the name of a vibe score if it's NULL at this point
  if (!subtitle) {
    subtitle = vibe_score_info_get_name(alerts_preferences_get_vibe_score_for_client(client));
    if (subtitle && IS_EMPTY_STRING(subtitle)) {
      subtitle = nullptr;
    }
  }
  menu_cell_basic_draw(ctx, cell_layer, pbl_i18n_get(title, data), pbl_i18n_get(subtitle, data),
                       nullptr);
}

static void prv_selection_changed_cb(SettingsCallbacks *context, uint16_t new_row,
                                     uint16_t old_row) {
  vibes_cancel();
  VibeScore *score;
  switch (new_row) {
#ifdef CONFIG_SPEAKER
    case VibeSettingsRow_MuteSpeaker:
    case VibeSettingsRow_SpeakerVolume: {
      // No vibe preview — this row controls a non-vibe setting.
      return;
    }
#endif
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
    case VibeSettingsRow_Hourly: {
      score = vibe_client_get_score(VibeClient_Hourly);
      break;
    }
    case VibeSettingsRow_OnDisconnect: {
      score = vibe_client_get_score(VibeClient_OnDisconnect);
      break;
    }
    case VibeSettingsRow_System: {
      // Vibe a short pulse so the user can feel the current system default vibe intensity
      vibes_short_pulse();
      // Just return because the remainder of this function only applies to vibe scores
      return;
    }
    default:
      pbl_panic();
  }
  if (!score) {
    PBL_LOG_ERR("Null VibeScore!");
    return;
  }
  vibe_score_do_vibe(score);
  vibe_score_destroy(score);
}

// Vibe score menu
/////////////////////////////

typedef struct VibeScoreMenuData {
  VibeClient client;
  bool committed;
  VibeScoreId *ids;
  const char **names;
} VibeScoreMenuData;

static void prv_preview_score(VibeScoreId id) {
  vibes_cancel();
  if (id == VibeScoreId_Disabled) {
    return;
  }
  VibeScore *score = vibe_score_create_with_resource(vibe_score_info_get_resource_id(id));
  if (!score) {
    return;
  }
  vibe_score_do_vibe(score);
  vibe_score_destroy(score);
}

static void prv_score_menu_selection_will_change(OptionMenu *option_menu, uint16_t new_row,
                                                 uint16_t old_row, void *context) {
  VibeScoreMenuData *data = settings_option_menu_get_context(context);
  if (new_row != old_row) {
    prv_preview_score(data->ids[new_row]);
  }
}

static void prv_score_menu_select(OptionMenu *option_menu, int selection, void *context) {
  VibeScoreMenuData *data = settings_option_menu_get_context(context);
  data->committed = true;
  alerts_preferences_set_vibe_score_for_client(data->client, data->ids[selection]);
  settings_menu_mark_dirty(SettingsMenuItemVibrations);
  prv_preview_score(data->ids[selection]);
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_score_menu_unload(OptionMenu *option_menu, void *context) {
  VibeScoreMenuData *data = settings_option_menu_get_context(context);
  // Keep the confirmation vibe of a selection, stop a preview when backing out
  if (!data->committed) {
    vibes_cancel();
  }
  app_free(data->ids);
  app_free(data->names);
  app_free(data);
}

static void prv_score_menu_push(VibeClient client, const char *title) {
  const unsigned int num_scores = vibe_score_info_get_ids_for_client(client, nullptr, 0);
  if (num_scores == 0) {
    return;
  }

  VibeScoreMenuData *data = app_zalloc_check(sizeof(*data));
  data->client = client;
  data->ids = app_malloc_check(num_scores * sizeof(*data->ids));
  data->names = app_malloc_check(num_scores * sizeof(*data->names));
  vibe_score_info_get_ids_for_client(client, data->ids, num_scores);

  const VibeScoreId current = alerts_preferences_get_vibe_score_for_client(client);
  int choice = 0;
  for (unsigned int i = 0; i < num_scores; i++) {
    data->names[i] = vibe_score_info_get_name(data->ids[i]);
    if (data->ids[i] == current) {
      choice = i;
    }
  }

  const OptionMenuCallbacks callbacks = {
    .select = prv_score_menu_select,
    .selection_will_change = prv_score_menu_selection_will_change,
    .unload = prv_score_menu_unload,
  };
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, choice, &callbacks, num_scores,
                            true /* icons_enabled */, data->names, data);
}

// System vibe intensity menu
/////////////////////////////

static const char *s_intensity_names[VibeIntensityNum];

static void prv_intensity_menu_selection_will_change(OptionMenu *option_menu, uint16_t new_row,
                                                     uint16_t old_row, void *context) {
  if (new_row != old_row) {
    // Apply the intensity until the menu closes so the user can feel it
    vibes_cancel();
    vibe_intensity_set(new_row);
    vibes_short_pulse();
  }
}

static void prv_intensity_menu_select(OptionMenu *option_menu, int selection, void *context) {
  alerts_preferences_set_vibe_intensity(selection);
  settings_menu_mark_dirty(SettingsMenuItemVibrations);
  vibes_cancel();
  vibe_intensity_set(selection);
  vibes_short_pulse();
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_intensity_menu_unload(OptionMenu *option_menu, void *context) {
  vibe_intensity_set(alerts_preferences_get_vibe_intensity());
}

static void prv_intensity_menu_push(const char *title) {
  for (int i = 0; i < VibeIntensityNum; i++) {
    s_intensity_names[i] = vibe_intensity_get_string_for_intensity(i);
  }
  const OptionMenuCallbacks callbacks = {
    .select = prv_intensity_menu_select,
    .selection_will_change = prv_intensity_menu_selection_will_change,
    .unload = prv_intensity_menu_unload,
  };
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, vibe_intensity_get(),
                            &callbacks, VibeIntensityNum, true /* icons_enabled */,
                            s_intensity_names, nullptr);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  vibes_cancel();

  VibeClient client;
  switch (row) {
#ifdef CONFIG_SPEAKER
    case VibeSettingsRow_MuteSpeaker: {
      const bool new_muted = !alerts_preferences_get_speaker_muted();
      alerts_preferences_set_speaker_muted(new_muted);
      speaker_service_handle_audio_prefs_changed();
      settings_menu_mark_dirty(SettingsMenuItemVibrations);
      return;
    }
    case VibeSettingsRow_SpeakerVolume: {
      speaker_volume_window_push();
      return;
    }
#endif
    case VibeSettingsRow_Notifications: {
      client = VibeClient_Notifications;
      break;
    }
    case VibeSettingsRow_PhoneCalls: {
      client = VibeClient_PhoneCalls;
      break;
    }
    case VibeSettingsRow_Alarms: {
      client = VibeClient_Alarms;
      break;
    }
    case VibeSettingsRow_Hourly: {
      client = VibeClient_Hourly;
      break;
    }
    case VibeSettingsRow_OnDisconnect: {
      client = VibeClient_OnDisconnect;
      break;
    }
    case VibeSettingsRow_System: {
      prv_intensity_menu_push(prv_row_title(row));
      return;
    }
    default:
      pbl_panic();
  }

  prv_score_menu_push(client, prv_row_title(row));
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

  data->callbacks = (SettingsCallbacks){
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
#ifdef CONFIG_SPEAKER
    .name = pbl_i18n_noop("Sounds & Haptics"),
#else
    .name = pbl_i18n_noop("Vibrations"),
#endif
    .init = prv_init,
  };

  return &s_module_info;
}
