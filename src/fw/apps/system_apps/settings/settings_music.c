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

#include "settings_music.h"
#include "settings_menu.h"
#include "settings_option_menu.h"
#include "settings_window.h"

#include "applib/ui/app_window_stack.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "util/size.h"

#include <stdbool.h>

typedef struct {
  SettingsCallbacks callbacks;
} SettingsMusicData;

enum MusicItem {
  MusicItemShowVolumeControls,
  MusicItemShowProgressBar,
  MusicItem_Count,
};

// Show Volume Controls
//////////////////////////

static bool prv_get_show_volume_controls(void) {
  return shell_prefs_get_music_show_volume_controls();
}

static void prv_toggle_show_volume_controls(void) {
  shell_prefs_set_music_show_volume_controls(!prv_get_show_volume_controls());
}

// Show Progress Bar
//////////////////////////

static bool prv_get_show_progress_bar(void) {
  return shell_prefs_get_music_show_progress_bar();
}

static void prv_toggle_show_progress_bar(void) {
  shell_prefs_set_music_show_progress_bar(!prv_get_show_progress_bar());
}

// Menu Layer Callbacks
////////////////////////

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return MusicItem_Count;
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsMusicData *data = ((SettingsOptionMenuData *)context)->context;
  const char *subtitle = NULL;
  const char *title = NULL;

  switch (row) {
    case MusicItemShowVolumeControls:
      title = i18n_noop("Volume Controls");
      subtitle = prv_get_show_volume_controls() ? i18n_noop("Show") : i18n_noop("Hide");
      break;
    case MusicItemShowProgressBar:
      title = i18n_noop("Progress Bar");
      subtitle = prv_get_show_progress_bar() ? i18n_noop("Show") : i18n_noop("Hide");
      break;
    default:
      WTF;
  }

  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsMusicData *data = (SettingsMusicData *)context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case MusicItemShowVolumeControls:
      prv_toggle_show_volume_controls();
      break;
    case MusicItemShowProgressBar:
      prv_toggle_show_progress_bar();
      break;
    default:
      WTF;
  }
  settings_menu_mark_dirty(SettingsMenuItemMusic);
}

static Window *prv_init(void) {
  SettingsMusicData* data = app_malloc_check(sizeof(*data));
  *data = (SettingsMusicData){};

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
  };

  return settings_window_create(SettingsMenuItemMusic, &data->callbacks);
}

const SettingsModuleMetadata *settings_music_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Music"),
    .init = prv_init,
  };

  return &s_module_info;
}
