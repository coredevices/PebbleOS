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

#include "mfg_runin_app.h"

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "drivers/battery.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/common/battery/battery_curve.h"
#include "system/logging.h"

#include <stdio.h>

typedef enum {
  RuninStateStart = 0,
  RuninStatePlugCharger,
  RuninStateRunning,
  RuninStatePass,
  RuninStateFail,
} RuninTestState;

static const char* status_text[] = {
  [RuninStateStart] =       "Start",
  [RuninStatePlugCharger] = "Plug Charger",
  [RuninStateRunning] =     "Running...",
  [RuninStatePass] =        "Pass",
  [RuninStateFail] =        "Fail",
};

#ifdef PLATFORM_TINTIN
static const int SLOW_THRESHOLD_PERCENTAGE = 42; // ~3850mv
static const int PASS_BATTERY_PERCENTAGE = 84; // ~4050mv
#elif defined(PLATFORM_ASTERIX)
static const int SLOW_THRESHOLD_PERCENTAGE = 0;
static const int PASS_BATTERY_PERCENTAGE = 70;
#else
static const int SLOW_THRESHOLD_PERCENTAGE = 0; // Always go "slow" on snowy
static const int PASS_BATTERY_PERCENTAGE = 60; // ~4190mv
#endif

typedef struct {
  Window window;

  TextLayer status;
  char status_string[20];

  TextLayer details;
  char details_string[64];

  RuninTestState test_state;
  uint32_t seconds_remaining;
  bool countdown_running;
  bool fastcharge_enabled;

  int pass_count;
} AppData;

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  RuninTestState next_state = data->test_state;

  const int charge_mv = battery_get_millivolts();
  const BatteryChargeState charge_state = battery_get_charge_state();

  switch (data->test_state) {
    case RuninStateStart:
      if (charge_state.is_plugged && charge_state.is_charging) {
        next_state = RuninStateRunning;
      } else {
        next_state = RuninStatePlugCharger;
      }
      break;
    case RuninStatePlugCharger:
      if (charge_state.is_plugged && charge_state.is_charging) {
        next_state = RuninStateRunning;
      }
      break;
    case RuninStateRunning:
      if (!data->countdown_running) {
        data->countdown_running = true;
      }
      if (!charge_state.is_plugged || !charge_state.is_charging) {
        data->pass_count = 0;
        next_state = RuninStatePlugCharger;
        break;
      }
      if (charge_state.charge_percent > SLOW_THRESHOLD_PERCENTAGE && data->fastcharge_enabled) {
        // go slow for a bit
        battery_set_fast_charge(false);
        data->fastcharge_enabled = false;
      } else if (charge_state.charge_percent > PASS_BATTERY_PERCENTAGE) {
        // The reading can be a bit shaky in the short term (i.e. a flaky USB connection), or we
        // just started charging. Make sure we have settled before transitioning into the
        // RuninStatePass state
        if (data->pass_count > 5) {
          next_state = RuninStatePass;

          data->countdown_running = false;
          // disable the charger so that we don't overcharge the battery
          battery_set_charge_enable(false);
        }
        data->pass_count++;
      } else {
        data->pass_count = 0;
      }
      break;
    case RuninStatePass:
    case RuninStateFail:
    default:
      break;
  }

  if (data->countdown_running) {
    --data->seconds_remaining;
    if (data->seconds_remaining == 0) {
      // Time's up!
      next_state = RuninStateFail;
      data->countdown_running = false;
      PBL_LOG(LOG_LEVEL_ERROR, "Failed runin testing");
    }
  }

  data->test_state = next_state;

  sniprintf(data->status_string, sizeof(data->status_string),
            "RUNIN\n%s", status_text[data->test_state]);
  text_layer_set_text(&data->status, data->status_string);

  int mins_remaining = data->seconds_remaining / 60;
  int secs_remaining = data->seconds_remaining % 60;
  sniprintf(data->details_string, sizeof(data->details_string),
            "Time:%02u:%02u\r\n%umV (%"PRIu8"%%)\r\nUSB: %s\r\nCharging: %s",
            mins_remaining, secs_remaining, charge_mv,
            charge_state.charge_percent,
            charge_state.is_plugged ? "yes" : "no",
            charge_state.is_charging ? "yes" : "no");
  text_layer_set_text(&data->details, data->details_string);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (!app_data->countdown_running &&
      (app_data->test_state == RuninStateStart ||
       app_data->test_state == RuninStatePlugCharger)) {

    // if the test has not yet started, it is ok to push the back button to leave.
    app_window_stack_pop(true);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (app_data->test_state == RuninStateFail || app_data->test_state == RuninStatePass) {
    // we've finished the runin test - long-press to close the app
    app_window_stack_pop(true);
  }
}

static void prv_config_provider(void *data) {
  window_long_click_subscribe(BUTTON_ID_SELECT, 3000, NULL, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void app_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  *data = (AppData) {
    .test_state = RuninStateStart,
    .countdown_running = false,
    .seconds_remaining = 5400, //1.5h
    .fastcharge_enabled = true,
    .pass_count = 0,
  };

  battery_set_fast_charge(true);
  battery_set_charge_enable(true);

  Window *window = &data->window;
  window_init(window, "Runin Test");

  TextLayer *status = &data->status;
  text_layer_init(status, &window->layer.bounds);
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, status_text[data->test_state]);
  layer_add_child(&window->layer, &status->layer);

  TextLayer *details = &data->details;
  text_layer_init(details,
                  &GRect(0, 65, window->layer.bounds.size.w, window->layer.bounds.size.h - 65));
  text_layer_set_font(details, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(details, GTextAlignmentCenter);
  layer_add_child(&window->layer, &details->layer);

  window_set_click_config_provider(window, prv_config_provider);
  window_set_fullscreen(window, true);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);

  app_window_stack_push(window, true /* Animated */);
}

static void s_main(void) {
  app_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_runin_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: fbb6d0e6-2d7d-40bc-8b01-f2f8beb9c394
    .common.uuid = { 0xfb, 0xb6, 0xd0, 0xe6, 0x2d, 0x7d, 0x40, 0xbc,
                     0x8b, 0x01, 0xf2, 0xf8, 0xbe, 0xb9, 0xc3, 0x94 },
    .name = "Runin App",
  };

  return (PebbleProcessMd*) &s_app_info;
}

