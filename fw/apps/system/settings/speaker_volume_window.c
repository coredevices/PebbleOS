/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "speaker_volume_window.h"

#ifdef CONFIG_SPEAKER

#include "menu.h"

#include "applib/ui/app_window_stack.h"
#include "applib/ui/value_picker_window.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/speaker/speaker_service.h"

#define VOLUME_STEP 5

typedef struct SpeakerVolumeWindowData {
  ValuePickerWindow picker_window;
} SpeakerVolumeWindowData;

static void prv_changed(ValuePickerWindow *picker_window, void *context) {
  // Restart the preview on every step so rapid changes aren't rejected as same-priority playback.
  speaker_service_stop_for_task(PebbleTask_App);
  speaker_service_set_owner_task(PebbleTask_App);
  speaker_service_play_volume_preview((uint8_t)value_picker_window_get_value(picker_window));
}

static void prv_selected(ValuePickerWindow *picker_window, void *context) {
  alerts_preferences_set_speaker_volume((uint8_t)value_picker_window_get_value(picker_window));
  speaker_service_handle_audio_prefs_changed();
  settings_menu_mark_dirty(SettingsMenuItemVibrations);
  app_window_stack_remove(&picker_window->window, true /* animated */);
}

static void prv_unload(ValuePickerWindow *picker_window, void *context) {
  speaker_service_stop_for_task(PebbleTask_App);
  i18n_free_all(context);
  app_free(context);
}

void speaker_volume_window_push(void) {
  SpeakerVolumeWindowData *data = app_zalloc_check(sizeof(*data));
  const ValuePickerContent content = {
    .title = i18n_get("Volume", data),
    .unit = "%",
    .value = alerts_preferences_get_speaker_volume(),
    .min_value = 0,
    .max_value = 100,
    .step = VOLUME_STEP,
  };
  value_picker_window_init(&data->picker_window, &content, NULL,
                           (ValuePickerWindowCallbacks){
                             .changed = prv_changed,
                             .selected = prv_selected,
                             .unload = prv_unload,
                           },
                           data);
  app_window_stack_push(&data->picker_window.window, true /* animated */);
}

#endif // CONFIG_SPEAKER
