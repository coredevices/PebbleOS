/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_discharge.h"

#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "applib/tick_timer_service.h"
#include "console/console_internal.h"
#include "drivers/battery.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/common/bluetooth/bluetooth_ctl.h"
#include "system/logging.h"

typedef enum {
  DischargeStateWaitUnplug = 0,
  DischargeStateDischarging,
} DischargeTestState;

static const char *status_text[] = {
    [DischargeStateWaitUnplug] = "Unplug Watch",
    [DischargeStateDischarging] = "Discharging",
};

typedef struct {
  Window window;

  TextLayer status;
  char status_string[64];

  TextLayer details;
  char details_string[256];

  DischargeTestState test_state;

  // Battery state at start of discharge phase
  int32_t initial_voltage_mv;
  uint8_t initial_percent;

  // Elapsed time in seconds
  uint32_t elapsed_seconds;
} AppData;

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  // Get battery state
  BatteryConstants battery_const;
  battery_get_constants(&battery_const);
  const BatteryChargeState charge_state = battery_get_charge_state();

  switch (data->test_state) {
    case DischargeStateWaitUnplug:
      // Wait for user to unplug the watch
      if (!charge_state.is_plugged) {
        // Watch unplugged, record initial state and start discharging
        data->test_state = DischargeStateDischarging;
        data->initial_voltage_mv = battery_const.v_mv;
        data->initial_percent = charge_state.charge_percent;
        data->elapsed_seconds = 0;

        // Disable serial console input (RX) may consume too much power on some systems
        serial_console_set_rx_enabled(false);

        // Disable Bluetooth
        bt_ctl_set_enabled(false);
      }
      break;

    case DischargeStateDischarging:
      // Increment elapsed time
      data->elapsed_seconds++;
      break;
  }

  // Update status display
  sniprintf(data->status_string, sizeof(data->status_string), "DISCHARGE\n%s",
            status_text[data->test_state]);
  text_layer_set_text(&data->status, data->status_string);

  // Update details display based on state
  switch (data->test_state) {
    case DischargeStateWaitUnplug:
      sniprintf(data->details_string, sizeof(data->details_string), "Unplug charger\nto begin test");
      break;

    case DischargeStateDischarging: {
      int hours = data->elapsed_seconds / 3600;
      int mins = (data->elapsed_seconds % 3600) / 60;
      int secs = data->elapsed_seconds % 60;
      int32_t voltage_delta = battery_const.v_mv - data->initial_voltage_mv;
      int8_t percent_delta = (int8_t)charge_state.charge_percent - (int8_t)data->initial_percent;

      sniprintf(data->details_string, sizeof(data->details_string),
                "Elapsed: %02d:%02d:%02d\n\n"
                "Current:\n"
                "%" PRId32 "mV %" PRIu8 "%%\n"
                "Delta:\n"
                "%" PRId32 "mV  %" PRId8 "%%",
                hours, mins, secs,
                battery_const.v_mv, charge_state.charge_percent,
                voltage_delta, percent_delta);
      break;
    }
  }

  text_layer_set_text(&data->details, data->details_string);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *data) {
  // Restore serial console RX
  serial_console_set_rx_enabled(true);

  // Re-enable Bluetooth
  bt_ctl_set_enabled(true);

  app_window_stack_pop(true);
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void app_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  *data = (AppData){
      .test_state = DischargeStateWaitUnplug,
      .initial_voltage_mv = 0,
      .initial_percent = 0,
      .elapsed_seconds = 0,
  };

  Window *window = &data->window;
  window_init(window, "Discharge Test");

  TextLayer *status = &data->status;
  text_layer_init(status, &window->layer.bounds);
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, status_text[data->test_state]);
  layer_add_child(&window->layer, &status->layer);

  TextLayer *details = &data->details;
  text_layer_init(details,
                  &GRect(0, 65, window->layer.bounds.size.w, window->layer.bounds.size.h - 65));
  text_layer_set_font(details, fonts_get_system_font(FONT_KEY_GOTHIC_18));
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

const PebbleProcessMd *mfg_discharge_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common.main_func = &s_main,
      // UUID: a1b2c3d4-e5f6-4789-a0b1-c2d3e4f56789
      .common.uuid = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x47, 0x89, 0xa0, 0xb1, 0xc2, 0xd3, 0xe4,
                      0xf5, 0x67, 0x89},
      .name = "Discharge",
  };

  return (PebbleProcessMd *)&s_app_info;
}
