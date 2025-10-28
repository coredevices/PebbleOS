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

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "drivers/vibe.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "mfg/results_ui.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"

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

  //! How many times we've vibrated
  int wait;
  VibeTestState state;

#if MFG_INFO_RECORDS_TEST_RESULTS
  MfgResultsUI results_ui;
#endif
} AppData;

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
      data->state = STATE_ERROR;
      tick_timer_service_unsubscribe();
    }
  } else if (data->state == STATE_WAITING) {
    data->wait++;
    if (data->wait >= WAIT_AFTER_CALIBRATION_S) {
      text_layer_set_text(&data->status, "VIBING...");
      data->state = STATE_VIBING;
    }
  } else if (data->state == STATE_VIBING) {
    vibes_short_pulse();
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
  text_layer_set_text(title, "VIBE TEST");
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

const PebbleProcessMd* mfg_vibe_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: f676085a-b130-4492-b6a1-85492602ba00
    .common.uuid = { 0xf6, 0x76, 0x08, 0x5a, 0xb1, 0x30, 0x44, 0x92,
                     0xb6, 0xa1, 0x85, 0x49, 0x26, 0x02, 0xba, 0x00 },
    .name = "MfgVibe",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

