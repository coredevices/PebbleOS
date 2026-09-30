/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! The Quick Launch action that switches the button lock's auto-lock on and
//! off. It only ever touches the pause pref, never the hold duration, so the
//! Back+Down combo keeps working in both positions.

#include "button_lock_auto.h"

#include "applib/app.h"
#include "applib/ui/action_toggle.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/i18n/i18n.h"
#include "resource/resource_ids.auto.h"
#include "shell/normal/button_lock.h"
#include "shell/prefs.h"

#define UNAVAILABLE_DIALOG_TIMEOUT_MS (3000)

static bool prv_get_state(void *context) {
  return !shell_prefs_get_button_lock_auto_paused();
}

static void prv_set_state(bool enabled, void *context) {
  shell_prefs_set_button_lock_auto_paused(!enabled);
  button_lock_handle_prefs_changed();
}

static const ActionToggleImpl s_button_lock_auto_action_toggle_impl = {
  .window_name = "Auto-Lock Toggle",
  .prompt_icon = RESOURCE_ID_BUTTON_LOCK,
  .result_icon = RESOURCE_ID_BUTTON_LOCK,
  .result_icon_static = true,
  .prompt_enable_message = i18n_noop("Turn On Auto-Lock?"),
  .prompt_disable_message = i18n_noop("Turn Off Auto-Lock?"),
  .result_enable_message = i18n_noop("Auto-Lock\nOn"),
  .result_disable_message = i18n_noop("Auto-Lock\nOff"),
  .callbacks = {
    .get_state = prv_get_state,
    .set_state = prv_set_state,
  },
};

//! Auto-lock has to be set up in Settings first: there is nothing to switch on
//! or off until both the unlock combo and a duration are configured.
static bool prv_is_configured(void) {
  return (shell_prefs_get_button_lock_hold_ms() != 0) &&
         (shell_prefs_get_button_lock_auto_ms() != 0);
}

static void prv_push_unavailable_dialog(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Auto-Lock Unavailable");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  const char *msg = i18n_get(i18n_noop("Set up Auto-Lock in Settings"), dialog);
  dialog_set_text(dialog, msg);
  i18n_free(msg, dialog);
  dialog_set_icon(dialog, RESOURCE_ID_BUTTON_LOCK);
  dialog_set_timeout(dialog, UNAVAILABLE_DIALOG_TIMEOUT_MS);
  simple_dialog_push(simple_dialog, app_state_get_window_stack());
}

static void prv_main(void) {
  if (prv_is_configured()) {
    action_toggle_push(&(ActionToggleConfig){
      .impl = &s_button_lock_auto_action_toggle_impl,
      .set_exit_reason = true,
    });
  } else {
    prv_push_unavailable_dialog();
  }
  app_event_loop();
}

const PebbleProcessMd *button_lock_auto_toggle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common =
        {
          .main_func = &prv_main,
          .uuid = BUTTON_LOCK_AUTO_TOGGLE_UUID,
          .visibility = ProcessVisibilityQuickLaunch,
        },
    /// The Quick Launch action that toggles the button lock's auto-lock.
    .name = i18n_noop("Toggle Auto-Lock"),
  };
  return &s_app_info.common;
}
