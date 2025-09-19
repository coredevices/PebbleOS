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

#include "crashed_ui.h"

#include "services/common/light.h"

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/window_stack.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/worker_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"

#include <stdio.h>

typedef struct {
  ActionableDialog *actionable_dialog;
  ActionBarLayer action_bar;
  GBitmap check_icon;
  GBitmap x_icon;
  AppInstallId app_install_id;
} WorkerCrashDialogData;

static void prv_worker_crash_dialog_unload(void *context) {
  WorkerCrashDialogData *data = context;
  action_bar_layer_deinit(&data->action_bar);
  gbitmap_deinit(&data->check_icon);
  gbitmap_deinit(&data->x_icon);
  kernel_free(data);
}

static WindowStack *prv_get_window_stack(void) {
  return modal_manager_get_window_stack(ModalPriorityAlert);
}

static void prv_worker_crash_button_up_handler(ClickRecognizerRef recognizer, void *context) {
  WorkerCrashDialogData *data = context;

  // Push an event to launch the app for the worker that crashed
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = data->app_install_id,
  });

  // Pop the worker crash dialog
  actionable_dialog_pop(data->actionable_dialog);
}

static void prv_worker_crash_button_down_handler(ClickRecognizerRef recognizer, void *context) {
  WorkerCrashDialogData *data = context;

  // Have the worker manager launch the next worker
  worker_manager_launch_next_worker(data->app_install_id);

  // Pop the worker crash dialog
  actionable_dialog_pop(data->actionable_dialog);
}

static void prv_worker_crash_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_worker_crash_button_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_worker_crash_button_down_handler);
}

//! Configure a crash dialog with the given crash reason text
//! @param dialog The dialog to configure
//! @param crash_reason The crash reason text
static void prv_configure_crash_dialog(Dialog *dialog, const char *crash_reason) {
  dialog_set_text(dialog, i18n_get(crash_reason, dialog));
  i18n_free(crash_reason, dialog);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_WARNING_TINY);
  dialog_set_vibe(dialog, true);
}

//! Construct a worker crash reason string
//! @param app_install_id The worker's install id
//! @return The resulting string, which must be freed by the caller
static char *prv_create_worker_crash_reason_string(AppInstallId app_install_id) {
  // Get the information about the app whose worker crashed
  AppInstallEntry entry;
  const bool app_found = app_install_get_entry_for_install_id(app_install_id, &entry);

  // Construct the crash reason string (copied by the dialog, so we don't have to free it ourselves)
  // "Worker " is 7, optional space is 1, up to 15 for app name, rest of string is 32, 1 for '\0'
  const uint8_t MAX_APP_NAME_STRING_LENGTH = 15;
  const uint8_t CRASH_REASON_BUFFER_SIZE = 7 + 1 + MAX_APP_NAME_STRING_LENGTH + 32 + 1;
  char *crash_reason = kernel_zalloc_check(CRASH_REASON_BUFFER_SIZE);
  const char *crash_str = i18n_noop("%s%.*s is not responding.\n\nOpen app?");
  sniprintf(crash_reason, CRASH_REASON_BUFFER_SIZE,
            i18n_get(crash_str, crash_reason),
            app_found ? " " : "",
            MAX_APP_NAME_STRING_LENGTH,
            app_found ? entry.name : "");
  i18n_free(crash_str, crash_reason);
  return crash_reason;
}

static void prv_push_worker_crash_dialog(void *context) {
  const AppInstallId app_install_id = (AppInstallId) context;

  WorkerCrashDialogData *data = kernel_zalloc_check(sizeof(WorkerCrashDialogData));
  data->app_install_id = app_install_id;

  // Initialize icons for the worker crash dialog's action bar
  GBitmap *check_icon = &data->check_icon;
  gbitmap_init_with_resource(check_icon, RESOURCE_ID_ACTION_BAR_ICON_CHECK);
  GBitmap *x_icon = &data->x_icon;
  gbitmap_init_with_resource(x_icon, RESOURCE_ID_ACTION_BAR_ICON_X);

  // Initialize and configure the worker crash dialog's action bar
  ActionBarLayer *action_bar = &data->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, check_icon);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, x_icon);
  action_bar_layer_set_click_config_provider(action_bar, prv_worker_crash_click_config_provider);
  action_bar_layer_set_context(action_bar, data);

  // crash_reason buffer is allocated on the heap and freed below
  char *crash_reason = prv_create_worker_crash_reason_string(app_install_id);

  // Create and configure the worker crash actionable dialog
  data->actionable_dialog = actionable_dialog_create("Crashed");
  if (!data->actionable_dialog) {
    // Just return and don't show any crash UI if we failed to create the actionable dialog
    return;
  }
  Dialog *dialog = actionable_dialog_get_dialog(data->actionable_dialog);
  prv_configure_crash_dialog(dialog, crash_reason);
  kernel_free(crash_reason);
  actionable_dialog_set_action_bar_type(data->actionable_dialog,
                                        DialogActionBarCustom,
                                        action_bar);
  DialogCallbacks callbacks = (DialogCallbacks) {
    .unload = prv_worker_crash_dialog_unload
  };
  dialog_set_callbacks(dialog, &callbacks, data);

  // Push the worker crash actionable dialog
  actionable_dialog_push(data->actionable_dialog, prv_get_window_stack());

  light_enable_interaction();
}

void crashed_ui_show_worker_crash(const AppInstallId install_id) {
  launcher_task_add_callback(prv_push_worker_crash_dialog, (void *) install_id);
}

// ---------------------------------------------------------------------------
#define CORE_DUMP_COMPLETE \
  i18n_noop("A bug report has been captured. " \
            "Please finish uploading the bug report using the Pebble phone app.")

#define YOUR_PEBBLE_RESET \
  i18n_noop("Your Pebble just reset. " \
            "Please report this using the 'Support' link in the Pebble phone app.")

#define PHONE_BT_CONTROLLER_WEDGED \
  i18n_noop("Bluetooth on your phone is in a high power state. " \
            "Please report this using 'Support' and reboot your phone.")

//! Display a dialog for watch reset or bluetooth being stuck.
static void prv_push_reset_dialog(void *context) {
  const char *crash_reason = context;

  ExpandableDialog *expandable_dialog = expandable_dialog_create("Reset");
  prv_configure_crash_dialog(expandable_dialog_get_dialog(expandable_dialog), crash_reason);
  expandable_dialog_show_action_bar(expandable_dialog, false);
  expandable_dialog_push(expandable_dialog, prv_get_window_stack());

  light_enable_interaction();
}

void crashed_ui_show_forced_core_dump(void) {
  launcher_task_add_callback(prv_push_reset_dialog, CORE_DUMP_COMPLETE);
}

#if (defined(SHOW_BAD_BT_STATE_ALERT) || defined(SHOW_PEBBLE_JUST_RESET_ALERT))

//! Restrict only to the two defines above
//! Show the "Your pebble has just reset"
void crashed_ui_show_pebble_reset(void) {
  launcher_task_add_callback(prv_push_reset_dialog, YOUR_PEBBLE_RESET);
}

//! Restrict only to the two defines above
//! Show the "Your Bluetooth is ..."
void crashed_ui_show_bluetooth_stuck(void) {
  launcher_task_add_callback(prv_push_reset_dialog, PHONE_BT_CONTROLLER_WEDGED);
}

#endif
