/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_battery_discharge_app.h"

#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/qr_code.h"
#include "applib/ui/window_private.h"
#include "applib/tick_timer_service.h"
#include "drivers/battery.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"

typedef enum {
  BattDischargeStateStart = 0,
  BattDischargeStateCharging,
  BattDischargeStateUnplugWatch,
  BattDischargeStateStationary,
  BattDischargeStateQRBeforeDischarge,
  BattDischargeStateDischarging,
  BattDischargeStatePass,
  BattDischargeStateFail,
  BattDischargeStateQRAfterTest,
} BattDischargeTestState;

static const char *status_text[] = {
    [BattDischargeStateStart] = "Start",
    [BattDischargeStateCharging] = "Charging...",
    [BattDischargeStateUnplugWatch] = "Unplug Watch",
    [BattDischargeStateStationary] = "Stationary",
    [BattDischargeStateQRBeforeDischarge] = "Press Center",
    [BattDischargeStateDischarging] = "Discharging",
    [BattDischargeStatePass] = "PASS",
    [BattDischargeStateFail] = "FAIL",
    [BattDischargeStateQRAfterTest] = "Press Center",
};

static const int CHARGE_TARGET_PERCENTAGE = 95;

// Pass/fail criteria (FIXME - adjust these values based on real-world testing)
// After 24h discharge, battery should be within these ranges
#define MIN_BATTERY_PERCENTAGE_AFTER_24H 70
#define MIN_BATTERY_VOLTAGE_AFTER_24H 3900

// Test phase durations
#define STATIONARY_DURATION_SECONDS (30 * 60)      // 30 minutes
#define DISCHARGE_DURATION_SECONDS (24 * 60 * 60)  // 24 hours

typedef struct {
  Window window;

  TextLayer status;
  char status_string[32];

  TextLayer details;
  char details_string[128];

  QRCode qr_code;
  TextLayer qr_text;
  char qr_data[128];  // Long-lived buffer for QR code data
  char qr_text_string[128];

  BattDischargeTestState test_state;
  uint32_t seconds_remaining;
  bool countdown_running;

  // Battery state at start of discharge phase
  int32_t discharge_start_voltage_mv;
  uint8_t discharge_start_percent;
  int32_t discharge_start_temp_mc;

  // Battery state at end of test
  int32_t test_end_voltage_mv;
  uint8_t test_end_percent;
  int32_t test_end_temp_mc;

  int pass_count;
} AppData;

static void prv_show_qr_code(AppData *data, int32_t voltage_mv, int32_t temp_mc, uint8_t percent) {
  // Format QR data: "V:4050mV T:25.5C P:85%"
  int32_t temp_c = temp_mc / 1000;
  uint8_t temp_c_frac = ((temp_mc > 0 ? temp_mc : -temp_mc) % 1000) / 100;
  sniprintf(data->qr_data, sizeof(data->qr_data),
            "V:%" PRId32 "mV T:%" PRId32 ".%" PRIu8 "C P:%" PRIu8 "%%", voltage_mv, temp_c,
            temp_c_frac, percent);

  // Set QR code data
  qr_code_set_data(&data->qr_code, data->qr_data, strlen(data->qr_data));

  // Format text display (same as QR data)
  sniprintf(data->qr_text_string, sizeof(data->qr_text_string), "%s", data->qr_data);
  text_layer_set_text(&data->qr_text, data->qr_text_string);

  // Show QR code and text, hide regular status/details
  text_layer_set_text(&data->status, "");
  text_layer_set_text(&data->details, "");
  layer_set_hidden(&data->status.layer, true);
  layer_set_hidden(&data->details.layer, true);
  layer_set_hidden(&data->qr_code.layer, false);
  layer_set_hidden(&data->qr_text.layer, false);
}

static void prv_hide_qr_code(AppData *data) {
  // Hide QR code and text, show regular status/details
  layer_set_hidden(&data->qr_code.layer, true);
  layer_set_hidden(&data->qr_text.layer, true);
  layer_set_hidden(&data->status.layer, false);
  layer_set_hidden(&data->details.layer, false);
}

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  BattDischargeTestState next_state = data->test_state;

  // Get battery state
  BatteryConstants battery_const;
  battery_get_constants(&battery_const);
  const BatteryChargeState charge_state = battery_get_charge_state();

  switch (data->test_state) {
    case BattDischargeStateStart:
      if (charge_state.is_plugged && charge_state.is_charging) {
        next_state = BattDischargeStateCharging;
      }
      break;

    case BattDischargeStateCharging:
      // Wait for battery to reach target percentage
      if (!charge_state.is_plugged || !charge_state.is_charging) {
        // Lost charging connection, go back to start
        next_state = BattDischargeStateStart;
        data->pass_count = 0;
        break;
      }

      if (charge_state.charge_percent >= CHARGE_TARGET_PERCENTAGE) {
        // Battery charged, verify stability
        if (data->pass_count > 5) {
          // Transition to unplug watch state
          next_state = BattDischargeStateUnplugWatch;

          // Disable charger
          battery_set_charge_enable(false);
          data->pass_count = 0;
        }
        data->pass_count++;
      } else {
        data->pass_count = 0;
      }
      break;

    case BattDischargeStateUnplugWatch:
      // Wait for user to unplug the watch
      if (!charge_state.is_plugged) {
        // Watch unplugged, transition to stationary phase
        next_state = BattDischargeStateStationary;

        // Initialize stationary phase countdown
        data->seconds_remaining = STATIONARY_DURATION_SECONDS;
        data->countdown_running = true;
      }
      break;

    case BattDischargeStateStationary:
      // Check if charger was plugged back in (should not happen)
      if (charge_state.is_plugged) {
        next_state = BattDischargeStateFail;
        data->countdown_running = false;
        break;
      }

      // Countdown the stationary phase
      if (data->countdown_running) {
        --data->seconds_remaining;
        if (data->seconds_remaining == 0) {
          // Stationary phase complete, show QR code before discharge
          next_state = BattDischargeStateQRBeforeDischarge;

          // Record starting battery state
          data->discharge_start_voltage_mv = battery_const.v_mv;
          data->discharge_start_percent = charge_state.charge_percent;
          data->discharge_start_temp_mc = battery_const.t_mc;

          data->countdown_running = false;

          // Display QR code with battery data
          prv_show_qr_code(data, data->discharge_start_voltage_mv, data->discharge_start_temp_mc,
                           data->discharge_start_percent);
        }
      }
      break;

    case BattDischargeStateQRBeforeDischarge:
      // Waiting for user to press center button to continue
      // No automatic state transition here
      break;

    case BattDischargeStateDischarging:
      // Check if charger was plugged back in (should not happen)
      if (charge_state.is_plugged) {
        next_state = BattDischargeStateFail;
        data->countdown_running = false;
        break;
      }

      // Countdown the discharge phase
      if (data->countdown_running) {
        --data->seconds_remaining;
        if (data->seconds_remaining == 0) {
          // 24h discharge complete, check pass/fail criteria
          data->countdown_running = false;

          // Record final battery state
          data->test_end_voltage_mv = battery_const.v_mv;
          data->test_end_percent = charge_state.charge_percent;
          data->test_end_temp_mc = battery_const.t_mc;

          // Check if battery is within acceptable range
          if (charge_state.charge_percent >= MIN_BATTERY_PERCENTAGE_AFTER_24H &&
              battery_const.v_mv >= MIN_BATTERY_VOLTAGE_AFTER_24H) {
            next_state = BattDischargeStatePass;
          } else {
            next_state = BattDischargeStateFail;
          }
        }
      }
      break;

    case BattDischargeStatePass:
    case BattDischargeStateFail:
    case BattDischargeStateQRAfterTest:
    default:
      // Terminal states - no automatic transitions
      break;
  }

  data->test_state = next_state;

  // Update status display
  sniprintf(data->status_string, sizeof(data->status_string), "BATT DISCHARGE\n%s",
            status_text[data->test_state]);
  text_layer_set_text(&data->status, data->status_string);

  // Update details display based on state
  switch (data->test_state) {
    case BattDischargeStateStart:
      sniprintf(data->details_string, sizeof(data->details_string), "Plug charger\nto begin test");
      break;

    case BattDischargeStateCharging: {
      int32_t temp_c = battery_const.t_mc / 1000;
      sniprintf(data->details_string, sizeof(data->details_string),
                "Target: >=%d%%\n%" PRId32 "mV  %" PRId32 "C  (%d%%)\nUSB: %s\nCharging: %s",
                CHARGE_TARGET_PERCENTAGE, battery_const.v_mv, temp_c, charge_state.charge_percent,
                charge_state.is_plugged ? "yes" : "no", charge_state.is_charging ? "yes" : "no");
      break;
    }

    case BattDischargeStateUnplugWatch: {
      int32_t temp_c = battery_const.t_mc / 1000;
      sniprintf(data->details_string, sizeof(data->details_string),
                "Charging complete\n%" PRId32 "mV  %" PRId32
                "C  (%d%%)\n\nUnplug charger\nto continue",
                battery_const.v_mv, temp_c, charge_state.charge_percent);
      break;
    }

    case BattDischargeStateStationary: {
      int hours = data->seconds_remaining / 3600;
      int mins = (data->seconds_remaining % 3600) / 60;
      int secs = data->seconds_remaining % 60;

      sniprintf(data->details_string, sizeof(data->details_string), "Time: %02d:%02d:%02d", hours,
                mins, secs);
      break;
    }

    case BattDischargeStateDischarging: {
      int hours = data->seconds_remaining / 3600;
      int mins = (data->seconds_remaining % 3600) / 60;
      int secs = data->seconds_remaining % 60;
      int32_t temp_c = battery_const.t_mc / 1000;
      int32_t current_ma = battery_const.i_ua / 1000;

      sniprintf(data->details_string, sizeof(data->details_string),
                "Time: %02d:%02d:%02d\n"
                "%" PRId32 "mV  %" PRId32 "mA\n%" PRId32 "C (%" PRIu8 "%%)",
                hours, mins, secs, battery_const.v_mv, current_ma, temp_c,
                charge_state.charge_percent);
      break;
    }

    case BattDischargeStatePass: {
      int32_t temp_c = battery_const.t_mc / 1000;
      int32_t current_ma = battery_const.i_ua / 1000;

      sniprintf(data->details_string, sizeof(data->details_string),
                "TEST PASSED!\n"
                "%" PRId32 "mV  %" PRId32 "mA\n%" PRId32 "C (%" PRIu8 "%%)",
                battery_const.v_mv, current_ma, temp_c, charge_state.charge_percent);
      break;
    }

    case BattDischargeStateFail: {
      int32_t temp_c = battery_const.t_mc / 1000;
      int32_t current_ma = battery_const.i_ua / 1000;

      sniprintf(data->details_string, sizeof(data->details_string),
                "TEST FAILED!\n"
                "%" PRId32 " %" PRId32 "mA\n%" PRId32 "C (%" PRIu8 "%%)",
                battery_const.v_mv, current_ma, temp_c, charge_state.charge_percent);
      break;
    }

    case BattDischargeStateQRBeforeDischarge:
    case BattDischargeStateQRAfterTest:
      // QR code is displayed instead of details
      // Don't update text layers here
      return;
  }

  text_layer_set_text(&data->details, data->details_string);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  battery_set_charge_enable(true);
  app_window_stack_pop(true);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  switch (app_data->test_state) {
    case BattDischargeStatePass:
    case BattDischargeStateFail:
      // Transition to QR state to show final battery data
      app_data->test_state = BattDischargeStateQRAfterTest;
      prv_show_qr_code(app_data, app_data->test_end_voltage_mv, app_data->test_end_temp_mc,
                       app_data->test_end_percent);
      break;

    case BattDischargeStateQRBeforeDischarge:
      // Start the discharge phase
      app_data->test_state = BattDischargeStateDischarging;
      app_data->seconds_remaining = DISCHARGE_DURATION_SECONDS;
      app_data->countdown_running = true;

      // Hide QR code and show regular display
      prv_hide_qr_code(app_data);
      break;

    case BattDischargeStateQRAfterTest:
      // Exit the app
      app_window_stack_pop(true);
      break;

    default:
      // No action for other states
      break;
  }
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void app_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  *data = (AppData){
      .test_state = BattDischargeStateStart,
      .countdown_running = false,
      .seconds_remaining = 0,
      .discharge_start_voltage_mv = 0,
      .discharge_start_percent = 0,
      .discharge_start_temp_mc = 0,
      .test_end_voltage_mv = 0,
      .test_end_percent = 0,
      .test_end_temp_mc = 0,
      .pass_count = 0,
  };

  // Enable charging initially
  battery_set_charge_enable(true);

  Window *window = &data->window;
  window_init(window, "Battery Discharge Test");

  TextLayer *status = &data->status;
  text_layer_init(status, &window->layer.bounds);
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, status_text[data->test_state]);
  layer_add_child(&window->layer, &status->layer);

  TextLayer *details = &data->details;
  text_layer_init(details,
                  &GRect(0, 65, window->layer.bounds.size.w, window->layer.bounds.size.h - 65));
  text_layer_set_font(details, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(details, GTextAlignmentCenter);
  layer_add_child(&window->layer, &details->layer);

  // Initialize QR code (hidden by default)
  QRCode *qr_code = &data->qr_code;
  qr_code_init_with_parameters(
      qr_code, &GRect(10, 10, window->layer.bounds.size.w - 20, window->layer.bounds.size.h - 30),
      "", 0, QRCodeECCMedium, GColorBlack, GColorWhite);
  layer_add_child(&window->layer, &qr_code->layer);
  layer_set_hidden(&qr_code->layer, true);

  // Initialize QR text layer (hidden by default)
  TextLayer *qr_text = &data->qr_text;
  text_layer_init_with_parameters(
      qr_text, &GRect(0, window->layer.bounds.size.h - 20, window->layer.bounds.size.w, 20), "",
      fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorBlack, GColorWhite, GTextAlignmentCenter,
      GTextOverflowModeTrailingEllipsis);
  layer_add_child(&window->layer, &qr_text->layer);
  layer_set_hidden(&qr_text->layer, true);

  window_set_click_config_provider(window, prv_config_provider);
  window_set_fullscreen(window, true);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);

  app_window_stack_push(window, true /* Animated */);
}

static void s_main(void) {
  app_init();

  app_event_loop();
}

const PebbleProcessMd *mfg_battery_discharge_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common.main_func = &s_main,
      // UUID: a1b2c3d4-e5f6-4789-a0b1-c2d3e4f56789
      .common.uuid = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x47, 0x89, 0xa0, 0xb1, 0xc2, 0xd3, 0xe4,
                      0xf5, 0x67, 0x89},
      .name = "Batt Discharge",
  };

  return (PebbleProcessMd *)&s_app_info;
}
