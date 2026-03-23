/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "apps/prf/mfg_test_result.h"
#include "drivers/vibe.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"

#define VIBE_COUNT 5
#define FAIL_DISPLAY_S 3

typedef enum {
  STATE_CALIBRATE,
  STATE_WAITING,
  STATE_VIBING,
  STATE_ERROR,
} VibeTestState;

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;

  int wait;
  int vibe_count;
  VibeTestState state;
} AppData;

static void prv_result_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool passed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  mfg_test_result_report(MfgTestId_Vibration, passed, 0);
  app_window_stack_pop(false);
}

static void prv_result_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_result_confirmed);
}

static void prv_show_result_dialog(void) {
  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Vibration Result");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, "Vibration OK?");

  confirmation_dialog_set_click_config_provider(confirmation_dialog, prv_result_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_confirmation_dialog_push(confirmation_dialog);
}

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  const int WAIT_AFTER_CALIBRATION_S = 3;

  if (data->state == STATE_CALIBRATE) {
    status_t status = vibe_calibrate();
    if (status == S_SUCCESS) {
      text_layer_set_text(&data->status, "CALIBRATED");
      data->state = STATE_WAITING;
    } else if (status == E_INVALID_OPERATION) {
      text_layer_set_text(&data->status, "CALIBRATION SKIPPED");
      data->state = STATE_WAITING;
    } else {
      text_layer_set_text(&data->status, "CALIBRATION FAILED");
      mfg_test_result_report(MfgTestId_Vibration, false, 0);
      data->state = STATE_ERROR;
      data->wait = 0;
    }
  } else if (data->state == STATE_WAITING) {
    data->wait++;
    if (data->wait >= WAIT_AFTER_CALIBRATION_S) {
      text_layer_set_text(&data->status, "VIBING...");
      data->state = STATE_VIBING;
      data->vibe_count = 0;
    }
  } else if (data->state == STATE_VIBING) {
    vibes_long_pulse();
    data->vibe_count++;
    if (data->vibe_count >= VIBE_COUNT) {
      tick_timer_service_unsubscribe();
      prv_show_result_dialog();
    }
  } else if (data->state == STATE_ERROR) {
    data->wait++;
    if (data->wait >= FAIL_DISPLAY_S) {
      tick_timer_service_unsubscribe();
      app_window_stack_pop(false);
    }
  }
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .wait = 0,
    .state = STATE_CALIBRATE,
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "VIBRATION TEST");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(5, 70,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 70));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  layer_add_child(&window->layer, &status->layer);

  app_window_stack_push(window, true /* Animated */);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_vibration_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: f676085a-b130-4492-b6a1-85492602ba00
    .common.uuid = { 0xf6, 0x76, 0x08, 0x5a, 0xb1, 0x30, 0x44, 0x92,
                     0xb6, 0xa1, 0x85, 0x49, 0x26, 0x02, 0xba, 0x00 },
    .name = "MfgVibration",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
