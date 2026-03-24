/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "applib/ui/window.h"
#include "apps/prf/mfg_test_result.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "services/common/bluetooth/bluetooth_ctl.h"
#include "services/common/bluetooth/local_id.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  Window window;
  char dialog_text[128];
} AppData;

static void prv_result_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool passed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  mfg_test_result_report(MfgTestId_Adv, passed, 0);
  app_window_stack_pop(false);
}

static void prv_result_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_result_confirmed);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "BLE Adv");
  window_set_fullscreen(window, true);

  char name[BT_DEVICE_NAME_BUFFER_SIZE];
  bt_local_id_copy_device_name(name, false);
  snprintf(data->dialog_text, sizeof(data->dialog_text),
           "BLE adv OK?\n\n%s", name);

  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("BLE Adv");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);
  dialog_set_text(dialog, data->dialog_text);

  confirmation_dialog_set_click_config_provider(confirmation_dialog, prv_result_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_window_stack_push(window, true);
  app_confirmation_dialog_push(confirmation_dialog);
}

static void s_main(void) {
  // Restart BLE so it begins advertising
  bt_ctl_set_enabled(false);
  bt_ctl_set_enabled(true);

  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd *mfg_adv_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common.main_func = &s_main,
      // UUID: 4c8e2a1f-7d3b-4f9e-b5a2-6e1c8d3f7a9b
      .common.uuid = {0x4c, 0x8e, 0x2a, 0x1f, 0x7d, 0x3b, 0x4f, 0x9e,
                      0xb5, 0xa2, 0x6e, 0x1c, 0x8d, 0x3f, 0x7a, 0x9b},
      .name = "MfgAdv",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
