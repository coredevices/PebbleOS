/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gateway_switch_popup.h"

#include "applib/ui/action_bar_layer.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "comm/ble/kernel_le_client/kernel_le_client.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/i18n/i18n.h"
#include "system/logging.h"

#include <btutil/bt_device.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  BTBondingID bonding_id;
  char device_name[BT_DEVICE_NAME_BUFFER_SIZE];
  ConfirmationDialog *dialog;
} GatewaySwitchArgs;

static bool s_is_on_screen = false;

static void prv_click_cb(ClickRecognizerRef recognizer, void *context) {
  GatewaySwitchArgs *args = (GatewaySwitchArgs *)context;
  confirmation_dialog_pop(args->dialog);
  if (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP) {
    kernel_le_client_set_active_gateway(args->bonding_id);
  }
  s_is_on_screen = false;
  kernel_free(args);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_click_cb);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click_cb);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_cb);
}

static void prv_show_kernelmain_cb(void *ctx) {
  GatewaySwitchArgs *args = (GatewaySwitchArgs *)ctx;

  if (s_is_on_screen) {
    kernel_free(args);
    return;
  }
  s_is_on_screen = true;

  ConfirmationDialog *dialog = confirmation_dialog_create("GatewaySwitch");
  args->dialog = dialog;

  Dialog *d = confirmation_dialog_get_dialog(dialog);

  char *msg = task_zalloc_check(DIALOG_MAX_MESSAGE_LEN);
  sniprintf(msg, DIALOG_MAX_MESSAGE_LEN,
            i18n_get("%s still connected. Set as gateway?", dialog),
            args->device_name);

  confirmation_dialog_set_click_config_provider(dialog, prv_click_config_provider);
  dialog_set_background_color(d, GColorCobaltBlue);
  dialog_set_text_color(d, GColorWhite);
  dialog_set_text(d, msg);

  task_free(msg);
  i18n_free_all(dialog);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(dialog);
  action_bar_layer_set_context(action_bar, args);

  confirmation_dialog_push(dialog,
      modal_manager_get_window_stack(ModalPriorityGeneric));
}

void gateway_switch_popup_show(BTBondingID bonding_id, const char *device_name) {
  GatewaySwitchArgs *args = kernel_zalloc_check(sizeof(*args));
  args->bonding_id = bonding_id;
  strncpy(args->device_name, device_name, sizeof(args->device_name) - 1);
  launcher_task_add_callback(prv_show_kernelmain_cb, args);
}
