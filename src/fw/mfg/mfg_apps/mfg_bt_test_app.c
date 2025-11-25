/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_bt_test_app.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "process_state/app_state/app_state.h"
#include "process_management/process_manager.h"

#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "services/common/bluetooth/bt_compliance_tests.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"
#include "system/passert.h"
#include "system/reboot_reason.h"
#include "system/reset.h"

EventServiceInfo bt_state_change_event_info;

typedef enum {
  BtTestStateInit = 0,
  BtTestStateStopped,
  BtTestStateStarting,
  BtTestStateStopping,
  BtTestStateStarted,
  BtTestStateFailed,
  BtTestStateResetting,

  BtTestStateNumStates,
} BtTestState;

static const char* status_text[] = {
  [BtTestStateInit] = "Initializing",
  [BtTestStateStopped] = "Stopped",
  [BtTestStateStarting] = "Starting",
  [BtTestStateStopping] = "Stopping",
  [BtTestStateStarted] = "Started",
  [BtTestStateFailed] = "Failed",
  [BtTestStateResetting] = "Resetting",
};

typedef struct {
  Window window;
  TextLayer title;
  TextLayer status;
  BtTestState test_state;
  TimerID reset_timer;
} AppData;

static void update_text_layers_callback(void *data) {
  AppData *app_data = data;
  TextLayer *status = &app_data->status;
  text_layer_set_text(status, status_text[app_data->test_state]);
}

static void prv_bt_event_handler(PebbleEvent *e, void* data) {
  AppData *app_data = (AppData*)data;

  switch (app_data->test_state) {
    case BtTestStateStarting: {
      PBL_ASSERTN(!bt_ctl_is_bluetooth_active());
      if (bt_test_bt_sig_rf_test_mode()) {
        app_data->test_state = BtTestStateStarted;
      } else {
        app_data->test_state = BtTestStateFailed;
      }
      break;
    }
    case BtTestStateStopping: {
      PBL_ASSERTN(bt_ctl_is_bluetooth_active());
      app_data->test_state = BtTestStateStopped;
      break;
    }
    case BtTestStateStopped:
    case BtTestStateStarted:
      break;
    default:
      WTF;
  }

  process_manager_send_callback_event_to_process(PebbleTask_App, update_text_layers_callback, (void*)data);
}

static void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  AppData *data = app_state_get_user_data();
  BtTestState new_state = data->test_state;

  PBL_ASSERTN(data->test_state < BtTestStateNumStates);

  switch (data->test_state) {
    case BtTestStateStopped: {
      new_state = BtTestStateStarting;
      bt_ctl_set_override_mode(BtCtlModeOverrideStop);
      break;
    }
    case BtTestStateStarted: {
      new_state = BtTestStateStopping;
      bt_ctl_set_override_mode(BtCtlModeOverrideRun);
      break;
    }
    case BtTestStateStarting:
    case BtTestStateStopping:
    default:
      break;
  }

  data->test_state = new_state;
  update_text_layers_callback(data);
}

static void bt_test_reset_callback(void *timer_data) {
  RebootReason reason = { RebootReasonCode_MfgShutdown, 0 };
  reboot_reason_set(&reason);
  system_reset();
}

static void back_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  AppData *app_data = app_state_get_user_data();

  app_data->test_state = BtTestStateResetting;
  text_layer_set_text(&app_data->status, status_text[app_data->test_state]);

  if (app_data->reset_timer == TIMER_INVALID_ID) {
    bool success = false;
    app_data->reset_timer = new_timer_create();
    if (app_data->reset_timer == TIMER_INVALID_ID) {
      success = new_timer_start(app_data->reset_timer, 500, bt_test_reset_callback, app_data, 0 /*flags*/);
    }

    if (app_data->reset_timer == TIMER_INVALID_ID || !success) {
      bt_test_reset_callback(app_data);
    }
  }
}

static void config_provider(Window *window) {
  // single click / repeat-on-hold config:
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) back_single_click_handler);
}

static void handle_init() {
  AppData *data = task_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  *data = (AppData) {
    .test_state = BtTestStateInit,
    .reset_timer = TIMER_INVALID_ID,
  };

  Window *window = &data->window;
  window_init(window, "BT Test");

  // want to indicate resetting.
  window_set_overrides_back_button(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(title, "BT Test Mode");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(0, 50, window->layer.bounds.size.w, window->layer.bounds.size.h - 30));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(status, status_text[data->test_state]);
  layer_add_child(&window->layer, &status->layer);

  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);

  app_window_stack_push(window, true /* Animated */);

  bt_state_change_event_info = (EventServiceInfo) {
    .type = PEBBLE_BT_STATE_EVENT,
    .handler = prv_bt_event_handler,
    .context = data,
  };

  event_service_client_subscribe(&bt_state_change_event_info);
  bt_ctl_set_override_mode(BtCtlModeOverrideRun);
  bt_ctl_reset_bluetooth();
}

static void handle_deinit() {
  AppData *data = app_state_get_user_data();
  bt_ctl_set_override_mode(BtCtlModeOverrideNone);
  event_service_client_unsubscribe(&bt_state_change_event_info);
  task_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

static const PebbleProcessMdSystem s_mfg_bt_test_info = {
  .common.main_func = &s_main,
  .name = "BT Test"
};

const PebbleProcessMd* mfg_app_bt_test_get_info() {
  return (const PebbleProcessMd*) &s_mfg_bt_test_info;
}
