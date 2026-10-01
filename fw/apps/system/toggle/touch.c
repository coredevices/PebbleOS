/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch.h"

#include "applib/app.h"
#include "applib/ui/action_toggle.h"
#include "pbl/services/i18n/i18n.h"
#include "shell/prefs.h"

static bool prv_get_state(void *context) {
  return touch_is_globally_enabled();
}

static void prv_set_state(bool enabled, void *context) {
  touch_set_globally_enabled(enabled);
}

static const ActionToggleImpl s_touch_action_toggle_impl = {
  .window_name = "Touch Toggle",
  .prompt_icon = RESOURCE_ID_TOUCH,
  .result_icon = RESOURCE_ID_TOUCH,
  .prompt_enable_message = i18n_noop("Turn On Touch?"),
  .prompt_disable_message = i18n_noop("Turn Off Touch?"),
  .result_enable_message = i18n_noop("Touch On"),
  .result_disable_message = i18n_noop("Touch Off"),
  .callbacks = {
    .get_state = prv_get_state,
    .set_state = prv_set_state,
  },
};

static void prv_main(void) {
  action_toggle_push(&(ActionToggleConfig){
    .impl = &s_touch_action_toggle_impl,
    .set_exit_reason = true,
  });
  app_event_loop();
}

const PebbleProcessMd *touch_toggle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common =
        {
          .main_func = &prv_main,
          .uuid = TOUCH_TOGGLE_UUID,
          .visibility = ProcessVisibilityQuickLaunch,
        },
    /// The Quick Launch action that turns the touchscreen on or off.
    .name = i18n_noop("Toggle Touch"),
  };
  return &s_app_info.common;
}
