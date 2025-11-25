/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/battery_state_service.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/standby.h"
#include "process_management/pebble_process_md.h"
#include "process_management/worker_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/runlevel.h"
#include "services/common/status_led.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reboot_reason.h"
#include "system/reset.h"

static const uint32_t CHARGER_DISCONNECT_TIMEOUT_MS = 3000;

typedef enum DialogState {
  DialogState_Uninitialized = 0,
  DialogState_Charging,
  DialogState_FullyCharged,
} DialogState;


struct AppData {
  SimpleDialog *dialog;
  AppTimer *poweroff_timer;
  DialogState last_dialog_state;
  bool was_plugged;
};

static void prv_reboot_on_click(ClickRecognizerRef recognizer, void *data) {
  // Don't try to return to normal functioning; just reboot the watch. The user
  // thinks the watch is already off anyway.
  RebootReason reboot_reason = { RebootReasonCode_ShutdownMenuItem };
  reboot_reason_set(&reboot_reason);
  system_reset();
}

static void prv_config_provider(void *context) {
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    window_long_click_subscribe(i, 0, prv_reboot_on_click, NULL);
  }
}

static void prv_power_off_timer_expired(void *data) {
  enter_standby(RebootReasonCode_ShutdownMenuItem);
}

static void prv_battery_state_handler(BatteryChargeState charge) {
  struct AppData *data = app_state_get_user_data();
  if (charge.is_plugged && !data->was_plugged) {
    app_timer_cancel(data->poweroff_timer);
  } else if (!charge.is_plugged && data->was_plugged) {
    data->poweroff_timer = app_timer_register(
        CHARGER_DISCONNECT_TIMEOUT_MS, prv_power_off_timer_expired, NULL);
  }

  DialogState next_dialog_state = DialogState_Uninitialized;

  if (charge.is_charging) {
    next_dialog_state = DialogState_Charging;
  } else if (charge.is_plugged) {
    next_dialog_state = DialogState_FullyCharged;
  } else {
    // Unplugged. We'll be shutting down in a couple seconds if the user doesn't
    // plug the charger back in, so don't change the dialog.
    next_dialog_state = data->last_dialog_state;
  }

  Dialog *dialog = simple_dialog_get_dialog(data->dialog);

  if (next_dialog_state != data->last_dialog_state) {
    // Setting the dialog icon to itself restarts the animation, which looks
    // bad, so we want to avoid that if we can help it.
    switch (next_dialog_state) {
      case DialogState_FullyCharged:
        dialog_set_text(dialog, i18n_get("Fully Charged", data));
        dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_FULL_LARGE_INVERTED);
        break;
      case DialogState_Charging:
      default:
        dialog_set_text(dialog, i18n_get("Charging", data));
        dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_CHARGING_LARGE_INVERTED);
        break;
    }
  }

  if (charge.is_plugged) {
    if (charge.is_charging) {
      status_led_set(StatusLedState_Charging);
    } else {
      status_led_set(StatusLedState_FullyCharged);
    }
  } else {
    status_led_set(StatusLedState_Off);
  }

  data->was_plugged = charge.is_plugged;
  data->last_dialog_state = next_dialog_state;
}

static void prv_handle_init(void) {
  struct AppData *data = app_malloc_check(sizeof(struct AppData));
  *data = (struct AppData){};
  app_state_set_user_data(data);

  data->dialog = simple_dialog_create(WINDOW_NAME("Shutdown Charging"));
  Dialog *dialog = simple_dialog_get_dialog(data->dialog);
  dialog_set_background_color(dialog, GColorBlack);
  dialog_set_text_color(dialog, GColorWhite);
  window_set_click_config_provider(&dialog->window, prv_config_provider);

  // The assumption is that this app is launched when the charger is connected
  // and the shutdown menu item is selected.
  data->was_plugged = true;
  data->last_dialog_state = DialogState_Uninitialized;
  battery_state_service_subscribe(prv_battery_state_handler);
  // Handle the edge-case where the charger is disconnected between the user
  // selecting shut down and this app subscribing to battery state events.
  // Also set the initial battery charge level.
  prv_battery_state_handler(battery_state_service_peek());

  app_simple_dialog_push(data->dialog);
  // TODO: have the runlevel machinery disable bluetooth and worker.
  services_set_runlevel(RunLevel_BareMinimum);
  worker_manager_disable();
}


static void s_main(void) {
  prv_handle_init();
  app_event_loop();
}

const PebbleProcessMd* shutdown_charging_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      .main_func = s_main,
      .visibility = ProcessVisibilityHidden,
      // UUID: 48fa66c4-4e6f-4b32-bf75-a16e12d630c3
      .uuid = {0x48, 0xfa, 0x66, 0xc4, 0x4e, 0x6f, 0x4b, 0x32,
               0xbf, 0x75, 0xa1, 0x6e, 0x12, 0xd6, 0x30, 0xc3},
    },
    .name = "Shutdown Charging",
  };
  return (const PebbleProcessMd*) &s_app_md;
}
