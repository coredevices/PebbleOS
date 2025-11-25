/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// FIXME: Wha? We don't use this file directly.
#include "mfg_func_test_buttons.h"

#include "kernel/events.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "drivers/battery.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/new_timer/new_timer.h"

#include <stdio.h>

typedef struct {
  MfgFuncTestData *app_data;
  char text_top[32];
  TextLayer text_layer_top;
  TextLayer text_layer_center;
  char text_volt[16];
  TextLayer text_layer_volt;
  PathLayer bolt;
  TimerID poll_timer_id;
} BatteryTestData;

static void battery_polling_callback(void *timer_data) {
  BatteryTestData *data = (BatteryTestData*)timer_data;

  sprintf(data->text_volt, "%i mV", battery_get_millivolts());
  text_layer_set_text(&data->text_layer_volt, data->text_volt);

  const bool is_discharging = !battery_get_charge_state().is_charging;
  layer_set_hidden(&data->bolt.layer, is_discharging);
  layer_set_hidden(&data->text_layer_center.layer, !is_discharging);
}

static void stop_battery_polling(BatteryTestData *data) {
  if (data == NULL || data->poll_timer_id == TIMER_INVALID_ID) {
    return;
  }
  new_timer_delete(data->poll_timer_id);
  data->poll_timer_id = TIMER_INVALID_ID;
}

static void start_battery_polling(BatteryTestData *data) {
  if (data == NULL || data->poll_timer_id != TIMER_INVALID_ID) {
    stop_battery_polling(data);
  }
  const uint32_t poll_interval_ms = 300;
  if (data->poll_timer_id == TIMER_INVALID_ID) {
    data->poll_timer_id = new_timer_create();
  }
  new_timer_start(data->poll_timer_id, poll_interval_ms, battery_polling_callback, data, TIMER_START_FLAG_REPEATING);
}

static void battery_window_button_up(ClickRecognizerRef recognizer, void *context) {
  BatteryTestData *data = window_get_user_data(context);
  if (data->app_data->charge_test_done) {
    const bool animated = false;
    window_stack_pop(animated);
  } else {
    if (battery_get_charge_state().is_charging) {
      mfg_func_test_append_bits(MfgFuncTestBitChargeTestPassed);
      data->app_data->charge_test_done = true;
      stop_battery_polling(data);
      layer_set_hidden(&data->text_layer_volt.layer, true);
      layer_set_hidden(&data->bolt.layer, true);
      text_layer_set_text(&data->text_layer_center, "QC OK!");
      layer_set_hidden(&data->text_layer_center.layer, false);
    }
  }
}

static void battery_window_click_config_provider(void *context) {
  for (ButtonId button_id = BUTTON_ID_BACK; button_id < NUM_BUTTONS; ++button_id) {
    window_raw_click_subscribe(button_id, NULL, (ClickHandler) battery_window_button_up, context);
  }
}

static void battery_window_load(Window *window) {
  BatteryTestData *data = window_get_user_data(window);

  // Layout Battery Test Window:
  Layer *root = &window->layer;

  char addr_hex_str[BT_ADDR_FMT_BUFFER_SIZE_BYTES];
  bt_local_id_copy_address_hex_string(addr_hex_str);
  sprintf(data->text_top, "Quality Test\nMAC: %s", addr_hex_str);

  TextLayer *text_layer_top = &data->text_layer_top;
  text_layer_init(text_layer_top, &GRect(0, 0, 144, 168));
  text_layer_set_background_color(text_layer_top, GColorWhite);
  text_layer_set_text_color(text_layer_top, GColorBlack);
  text_layer_set_text(text_layer_top, data->text_top);
  text_layer_set_font(text_layer_top, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(root, &text_layer_top->layer);

  TextLayer *text_layer_center = &data->text_layer_center;
  text_layer_init(text_layer_center, &GRect(0, 60, 144, 40));
  text_layer_set_background_color(text_layer_center, GColorClear);
  text_layer_set_text_color(text_layer_center, GColorBlack);
  text_layer_set_text(text_layer_center, "Plug Charger");
  text_layer_set_font(text_layer_center, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(root, &text_layer_center->layer);

  TextLayer *text_layer_volt = &data->text_layer_volt;
  text_layer_init(text_layer_volt, &GRect(0, 128, 144, 40));
  text_layer_set_background_color(text_layer_volt, GColorBlack);
  text_layer_set_text_color(text_layer_volt, GColorWhite);
  text_layer_set_font(text_layer_volt, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(root, &text_layer_volt->layer);

  PathLayer *bolt = &data->bolt;
  path_layer_init(bolt, &BOLT_PATH_INFO);
  path_layer_set_fill_color(bolt, GColorBlack);
  path_layer_set_stroke_color(bolt, GColorClear);
  layer_set_frame(&bolt->layer, &GRect(58, 48, 28, 60));
  layer_set_hidden(&bolt->layer, true);
  layer_add_child(root, &bolt->layer);
}

static void battery_window_appear(Window *window) {
  BatteryTestData *data = window_get_user_data(window);
  start_battery_polling(data);
}

static void battery_window_disappear(Window *window) {
  BatteryTestData *data = window_get_user_data(window);
  stop_battery_polling(data);
}

static void push_battery_test_window(MfgFuncTestData *app_data) {
  static BatteryTestData s_battery_test_data;
  s_battery_test_data = (BatteryTestData) {
    .app_data = app_data,
    .poll_timer_id = TIMER_INVALID_ID,
  };

  // Battery Charge test window:
  Window *battery_window = &app_data->battery_window;
  window_init(battery_window, WINDOW_NAME("Mfg Func Test Battery"));
  window_set_overrides_back_button(battery_window, true);
  window_set_user_data(battery_window, app_data);
  window_set_click_config_provider_with_context(battery_window,
      (ClickConfigProvider) battery_window_click_config_provider, battery_window);
  window_set_window_handlers(battery_window, &(WindowHandlers) {
      .load = battery_window_load,
      .appear = battery_window_appear,
      .disappear = battery_window_disappear
  });
  window_set_user_data(battery_window, &s_battery_test_data);
  window_set_fullscreen(battery_window, true);

  const bool animated = false;
  window_stack_push(battery_window, animated);
}

