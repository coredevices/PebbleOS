/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "recordings.h"
#include "menu.h"
#include "option_menu.h"

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/ui.h"
#include "applib/voice/audio_recording.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/voice/voice_recording.h"
#include "resource/resource_ids.auto.h"
#include "shell/prefs.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef CONFIG_MIC

#define SETTINGS_RECORDINGS_MAX_SHOWN (64)
#define SETTINGS_RECORDINGS_REFRESH_MS (500)
#define VOICE_REC_ROW_OFFSET (1)

typedef enum {
  RecordingAction_PlayStop = 0,
  RecordingAction_Transcribe,
  RecordingAction_Delete,
  RecordingAction_Count,
} RecordingAction;

typedef struct {
  OptionMenu option_menu;
  VoiceRecordingInfo infos[SETTINGS_RECORDINGS_MAX_SHOWN];
  uint32_t count;
  VoiceRecordingId active_id;
  VoiceRecordingId selected_id;
  OptionMenu *action_menu;
  AppTimer *refresh_timer;
  bool was_recording;
  char row_buf[40];
} SettingsRecordingsData;

static const char *s_action_labels[] = {
    [RecordingAction_PlayStop] = i18n_noop("Play / Stop"),
    [RecordingAction_Transcribe] = i18n_noop("Transcribe"),
    [RecordingAction_Delete] = i18n_noop("Delete"),
};

static const char *prv_error_label(VoiceRecordingError error) {
  switch (error) {
    case VoiceRecordingError_Busy:
      return i18n_noop("Another audio operation is active.");
    case VoiceRecordingError_MicBusy:
      return i18n_noop("The microphone is busy.");
    case VoiceRecordingError_StorageFull:
      return i18n_noop("Recording storage is full.");
    case VoiceRecordingError_Codec:
      return i18n_noop("The audio codec could not start.");
    case VoiceRecordingError_NoSpace:
      return i18n_noop("Not enough flash space.");
    case VoiceRecordingError_FileOpen:
      return i18n_noop("The recording file could not be opened.");
    case VoiceRecordingError_Write:
    case VoiceRecordingError_Save:
      return i18n_noop("The recording could not be saved.");
    case VoiceRecordingError_MicStart:
      return i18n_noop("The microphone could not start.");
    case VoiceRecordingError_None:
    default:
      return i18n_noop("The operation could not be completed.");
  }
}

static void prv_show_dialog(const char *title, const char *text, bool success,
                            bool translate_text) {
  ExpandableDialog *expandable = expandable_dialog_create("Voice Memos Result");
  if (!expandable) {
    return;
  }

  Dialog *dialog = expandable_dialog_get_dialog(expandable);
  const char *localized_title = i18n_get(title, expandable);
  const char *localized_text = translate_text ? i18n_get(text, expandable) : text;
  expandable_dialog_set_header(expandable, localized_title);
  dialog_set_text(dialog, localized_text);
  dialog_set_icon(
      dialog, success ? RESOURCE_ID_GENERIC_CONFIRMATION_TINY : RESOURCE_ID_GENERIC_WARNING_TINY);
  dialog_set_background_color(dialog, GColorWhite);
  dialog_set_text_color(dialog, GColorBlack);
  expandable_dialog_set_select_action(expandable, RESOURCE_ID_ACTION_BAR_ICON_CHECK,
                                      expandable_dialog_close_cb);
  expandable_dialog_show_action_bar(expandable, true);
  i18n_free_all(expandable);
  app_expandable_dialog_push(expandable);
}

static void prv_reload(SettingsRecordingsData *data) {
  data->count = voice_recording_list(data->infos, SETTINGS_RECORDINGS_MAX_SHOWN);
  option_menu_reload_data(&data->option_menu);
}

static void prv_refresh_timer_cb(void *context) {
  SettingsRecordingsData *data = context;
  const bool recording = voice_recording_in_progress();
  if (recording != data->was_recording) {
    data->was_recording = recording;
    if (!recording) {
      data->active_id = VOICE_RECORDING_ID_INVALID;
    }
    prv_reload(data);
  }
}

static void prv_transcription_cb(AudioRecordingId recording_id, DictationSessionStatus status,
                                 char *transcription, void *context) {
  (void)recording_id;
  (void)context;
  if ((status == DictationSessionStatusSuccess) && transcription) {
    prv_show_dialog(i18n_noop("Transcription"), transcription, true, false);
  } else {
    prv_show_dialog(i18n_noop("Transcription failed"),
                    i18n_noop("Check the phone connection and try again."), false, true);
  }
}

static SettingsRecordingsData *prv_get_action_context(void *context) {
  return settings_option_menu_get_context(context);
}

static void prv_delete_cancel_cb(ClickRecognizerRef recognizer, void *context) {
  confirmation_dialog_pop(context);
}

static void prv_delete_confirm_cb(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *dialog = context;
  SettingsRecordingsData *data = actionable_dialog_get_user_data((ActionableDialog *)dialog);
  const bool deleted = voice_recording_delete(data->selected_id);
  confirmation_dialog_pop(dialog);

  if (deleted) {
    if (data->action_menu) {
      app_window_stack_remove(&data->action_menu->window, true);
    }
    prv_reload(data);
  } else {
    prv_show_dialog(i18n_noop("Delete failed"), i18n_noop("The recording is currently in use."),
                    false, true);
  }
}

static void prv_delete_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_delete_confirm_cb);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_delete_cancel_cb);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_delete_cancel_cb);
}

static void prv_push_delete_confirmation(SettingsRecordingsData *data) {
  ConfirmationDialog *confirmation = confirmation_dialog_create("Delete Voice Memo");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation);
  const char *text = i18n_get("Delete this voice memo?", confirmation);
  dialog_set_text(dialog, text);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_WARNING_TINY);
  dialog_set_background_color(dialog, GColorRed);
  dialog_set_text_color(dialog, GColorWhite);
  i18n_free_all(confirmation);

  actionable_dialog_set_user_data((ActionableDialog *)confirmation, data);
  confirmation_dialog_set_click_config_provider(confirmation, prv_delete_click_config);
  app_confirmation_dialog_push(confirmation);
}

static void prv_action_unload_cb(OptionMenu *option_menu, void *context) {
  SettingsRecordingsData *data = prv_get_action_context(context);
  voice_recording_stop_playback();
  data->action_menu = NULL;
}

static void prv_action_select_cb(OptionMenu *option_menu, int row, void *context) {
  SettingsRecordingsData *data = prv_get_action_context(context);

  switch ((RecordingAction)row) {
    case RecordingAction_PlayStop:
      if (voice_recording_is_playing()) {
        voice_recording_stop_playback();
      } else if (!voice_recording_play(data->selected_id)) {
        prv_show_dialog(i18n_noop("Playback failed"),
                        i18n_noop("The recording could not be played."), false, true);
      }
      break;
    case RecordingAction_Transcribe:
      voice_recording_stop_playback();
      if (!audio_recording_transcribe(data->selected_id, 0, prv_transcription_cb, data)) {
        prv_show_dialog(i18n_noop("Transcription unavailable"),
                        i18n_noop("Connect a phone with voice support and try again."), false,
                        true);
      }
      break;
    case RecordingAction_Delete:
      voice_recording_stop_playback();
      prv_push_delete_confirmation(data);
      break;
    case RecordingAction_Count:
      break;
  }
}

static void prv_push_action_menu(SettingsRecordingsData *data) {
  const OptionMenuCallbacks callbacks = {
      .unload = prv_action_unload_cb,
      .select = prv_action_select_cb,
  };
  data->action_menu = settings_option_menu_push(
      i18n_noop("Voice Memo"), OptionMenuContentType_SingleLine, OPTION_MENU_CHOICE_NONE,
      &callbacks, RecordingAction_Count, false, s_action_labels, data);
}

static uint16_t prv_get_num_rows_cb(OptionMenu *option_menu, void *context) {
  SettingsRecordingsData *data = context;
  return VOICE_REC_ROW_OFFSET + ((data->count == 0) ? 1 : data->count);
}

static void prv_draw_row_cb(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                            const GRect *text_frame, uint32_t row, bool selected, void *context) {
  SettingsRecordingsData *data = context;
  const char *title;

  if (row == 0) {
    title = voice_recording_in_progress() ? i18n_get("Stop recording", data)
                                          : i18n_get("New recording", data);
  } else if (data->count == 0) {
    title = i18n_get("No voice memos", data);
  } else {
    const uint32_t index = row - VOICE_REC_ROW_OFFSET;
    const VoiceRecordingInfo *info = &data->infos[index];
    const uint32_t secs = info->duration_ms / 1000;
    snprintf(data->row_buf, sizeof(data->row_buf), "Memo %" PRIu32 "  %" PRIu32 ":%02" PRIu32,
             index + 1, secs / 60, secs % 60);
    title = data->row_buf;
  }

  option_menu_system_draw_row(option_menu, ctx, cell_layer, text_frame, title, false, NULL);
}

static uint16_t prv_row_height_cb(OptionMenu *option_menu, uint16_t row, bool is_selected,
                                  void *context) {
  return option_menu_default_cell_height(option_menu->content_type, is_selected);
}

static void prv_select_cb(OptionMenu *option_menu, int row, void *context) {
  SettingsRecordingsData *data = context;

  if (row == 0) {
    if (voice_recording_in_progress()) {
      if (!voice_recording_stop(data->active_id)) {
        prv_show_dialog(i18n_noop("Recording failed"),
                        prv_error_label(voice_recording_last_error()), false, true);
      }
      data->active_id = VOICE_RECORDING_ID_INVALID;
    } else {
      data->active_id = voice_recording_start();
      if (data->active_id == VOICE_RECORDING_ID_INVALID) {
        prv_show_dialog(i18n_noop("Recording unavailable"),
                        prv_error_label(voice_recording_last_error()), false, true);
      }
    }
    data->was_recording = voice_recording_in_progress();
    prv_reload(data);
    return;
  }

  if (data->count == 0) {
    return;
  }

  data->selected_id = data->infos[row - VOICE_REC_ROW_OFFSET].id;
  prv_push_action_menu(data);
}

static void prv_unload_cb(OptionMenu *option_menu, void *context) {
  SettingsRecordingsData *data = context;
  if (data->refresh_timer) {
    app_timer_cancel(data->refresh_timer);
  }
  voice_recording_stop_playback();
  if (voice_recording_in_progress()) {
    voice_recording_stop_active();
  }
  option_menu_deinit(&data->option_menu);
  i18n_free_all(data);
  app_free(data);
}

static Window *prv_init(void) {
  SettingsRecordingsData *data = app_zalloc_check(sizeof(SettingsRecordingsData));

  const OptionMenuCallbacks callbacks = {
      .unload = prv_unload_cb,
      .draw_row = prv_draw_row_cb,
      .select = prv_select_cb,
      .get_num_rows = prv_get_num_rows_cb,
      .get_cell_height = prv_row_height_cb,
  };

  option_menu_init(&data->option_menu);
  option_menu_set_status_colors(&data->option_menu, GColorWhite, GColorBlack);
  const GColor highlight_bg = shell_prefs_get_theme_highlight_color();
  option_menu_set_highlight_colors(&data->option_menu, highlight_bg,
                                   gcolor_legible_over(highlight_bg));
  option_menu_set_title(&data->option_menu, i18n_get("Voice Memos", data));
  option_menu_set_content_type(&data->option_menu, OptionMenuContentType_SingleLine);
  option_menu_set_callbacks(&data->option_menu, &callbacks, data);

  data->active_id = VOICE_RECORDING_ID_INVALID;
  data->selected_id = VOICE_RECORDING_ID_INVALID;
  data->was_recording = voice_recording_in_progress();
  data->refresh_timer = app_timer_register_repeatable(SETTINGS_RECORDINGS_REFRESH_MS,
                                                      prv_refresh_timer_cb, data, true);
  prv_reload(data);

  return &data->option_menu.window;
}

const SettingsModuleMetadata *settings_recordings_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
      .name = i18n_noop("Voice Memos"),
      .init = prv_init,
  };

  return &s_module_info;
}

#endif  // CONFIG_MIC
