/*
 * Copyright 2025 Elad Dvash
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

#include "backlight_state.h"

#include "applib/app.h"
#include "applib/ui/action_toggle.h"
#include "process_management/app_manager.h"
#include "services/common/i18n/i18n.h"
#include "shell/prefs.h"

static bool prv_get_state(void *context) {
  return backlight_is_enabled();
}

static void prv_set_state(bool enabled, void *context) {
  backlight_set_enabled(enabled);
}

static const ActionToggleImpl s_backlight_state_action_toggle_impl = {
  .window_name = "Backlight Toggle",
  .prompt_icon = RESOURCE_ID_BACKLIGHT,
  .result_icon = RESOURCE_ID_BACKLIGHT,
  .prompt_enable_message = i18n_noop("Turn On Backlight?"),
  .prompt_disable_message = i18n_noop("Turn Off Backlight?"),
  .result_enable_message = i18n_noop("Backlight On"),
  .result_disable_message = i18n_noop("Backlight Off"),
  .callbacks = {
    .get_state = prv_get_state,
    .set_state = prv_set_state,
  },
};

static void prv_main(void) {
  action_toggle_push(&(ActionToggleConfig) {
    .impl = &s_backlight_state_action_toggle_impl,
    .set_exit_reason = true,
  });
  app_event_loop();
}

const PebbleProcessMd *backlight_state_toggle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      .main_func = &prv_main,
      .uuid = BACKLIGHT_STATE_TOGGLE_UUID,
      .visibility = ProcessVisibilityQuickLaunch,
    },
    .name = i18n_noop("Backlight"),
  };
  return &s_app_info.common;
}
