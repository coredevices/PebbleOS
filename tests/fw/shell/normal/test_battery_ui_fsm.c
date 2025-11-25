/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <stdio.h>
#include <string.h>

#include "applib/ui/vibes.h"
#include "apps/system_app_ids.h"
#include "kernel/low_power.h"
#include "kernel/ui/modals/modal_manager.h"
#include "kernel/util/standby.h"
#include "process_management/app_manager.h"
#include "services/common/battery/battery_curve.h"
#include "services/common/status_led.h"
#include "shell/normal/battery_ui.h"
#include "util/ratio.h"

extern void battery_ui_reset_fsm_for_tests(void);

// Stubs and fakes
/////////////////////////////////////////////////////////////////////////

#include "stubs_logging.h"
#include "stubs_vibe_intensity.h"
#include "stubs_vibe_pattern.h"

typedef enum PowerState {
  PowerGood,
  PowerLow,
  PowerCritical
} PowerState;

static PowerState s_state;
static bool s_entered_standby;
static bool s_dnd_on;
static uint8_t s_vibe_count;
static bool s_modal_onscreen;
static uint8_t s_modal_percent;
static bool s_modal_charging;
static bool s_low_power;
static bool s_critical;
static bool s_shutdown_charging;

void prv_set_state(PowerState state) {
  s_state = state;
}

bool battery_monitor_critical_lockout(void) {
  return s_state == PowerCritical;
}

bool low_power_is_active(void) {
  return s_state == PowerLow;
}

void enter_standby(RebootReasonCode reason) {
  s_entered_standby = true;
}

bool do_not_disturb_is_active(void) {
  return s_dnd_on;
}

void vibes_short_pulse(void) {
  s_vibe_count++;
}

void watchface_start_low_power(void) {
  s_low_power = true;
}

void watchface_launch_default(const CompositorTransition *animation) {
  s_low_power = false;
}

void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  if (config->id == APP_ID_BATTERY_CRITICAL) {
    s_critical = true;
  } else {
    s_shutdown_charging = true;
  }
}

void app_manager_close_current_app(bool gracefully) {
  if (s_critical) {
    s_critical = false;
  } else {
    s_shutdown_charging = false;
  }
}

void battery_ui_display_plugged(void) {
  s_modal_onscreen = true;
  s_modal_charging = true;
}

void battery_ui_display_fully_charged(void) {
  s_modal_onscreen = true;
  s_modal_charging = false;
}

void battery_ui_display_warning(uint32_t percent, BatteryUIWarningLevel warning_level) {
  s_modal_onscreen = true;
  s_modal_percent = percent;
}

void battery_ui_dismiss_modal(void) {
  s_modal_onscreen = false;
  s_modal_charging = false;
  s_modal_percent = 0;
}

void modal_manager_pop_all(void) {
}

void modal_manager_pop_all_below_priority(ModalPriority priority) {
}

void modal_manager_set_min_priority(ModalPriority priority) {
}

static PreciseBatteryChargeState prv_make_state(uint8_t percent, bool is_charging, bool is_plugged) {
  PreciseBatteryChargeState state = (PreciseBatteryChargeState) {
    .charge_percent = ratio32_from_percent(percent),
    .is_charging = is_charging,
    .is_plugged = is_plugged
  };

  return state;
}

static StatusLedState s_led_state;
void status_led_set(StatusLedState state) {
  s_led_state = state;
}

bool s_is_charging;
BatteryChargeState battery_get_charge_state(void) {
  // Don't bother setting other fields, they're not used.
  return (BatteryChargeState) { .is_charging = s_is_charging };
}

// Setup
////////////////////////////////////
void test_battery_ui_fsm__initialize(void) {
  prv_set_state(PowerGood);

  s_entered_standby = false;
  s_dnd_on = false;
  s_vibe_count = 0;
  s_modal_onscreen = false;
  s_modal_percent = 0;
  s_modal_charging = false;
  s_low_power = false;
  s_critical = false;
  s_shutdown_charging = false;
  s_led_state = StatusLedState_Off;
  s_is_charging = false;

  battery_ui_reset_fsm_for_tests();
}

void test_battery_ui_fsm__cleanup(void) {
}

// Helpers
////////////////////////////////////

void prv_change_state(PreciseBatteryChargeState new_state) {
  s_is_charging = new_state.is_charging;
  battery_ui_handle_state_change_event(new_state);
}

// Tests
////////////////////////////////////

void test_battery_ui_fsm__transitions(void) {
  PreciseBatteryChargeState charging = prv_make_state(100, true, true),
                            fully_charged = prv_make_state(100, false, true),
                            nop = prv_make_state(50, false, false);
  PreciseBatteryChargeState warning_18h =
      prv_make_state(battery_curve_get_percent_remaining(18), false, false);
  PreciseBatteryChargeState warning_12h =
      prv_make_state(battery_curve_get_percent_remaining(12), false, false);

  // Good - shouldn't do anything
  prv_change_state(nop);
  cl_assert(!s_modal_onscreen && !s_low_power && !s_critical);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Charging - should open charging modal
  prv_change_state(charging);
  cl_assert(s_modal_onscreen && s_modal_charging);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // Fully charged - should trigger another event, opening fully charged modal
  prv_change_state(fully_charged);
  cl_assert(s_modal_onscreen && !s_modal_charging);
  cl_assert_equal_i(s_led_state, StatusLedState_FullyCharged);

  // Back to good - modal should have closed
  prv_change_state(nop);
  cl_assert(!s_modal_onscreen);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Warning - Should trigger various modals
  prv_change_state(warning_18h);
  cl_assert(s_modal_onscreen && s_modal_percent == battery_curve_get_percent_remaining(18));
  prv_change_state(warning_12h);
  cl_assert(s_modal_onscreen && s_modal_percent == battery_curve_get_percent_remaining(12));
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Low Power - should enter low power watchface, modal should have closed
  prv_set_state(PowerLow);
  prv_change_state(nop);
  cl_assert(!s_modal_onscreen && s_low_power);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Critical - should enter critical app, low power should have closed
  prv_set_state(PowerCritical);
  prv_change_state(nop);
  cl_assert(!s_low_power && s_critical);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Charging - critical should disable, modal should appear
  prv_set_state(PowerGood);
  prv_change_state(charging);
  cl_assert(!s_critical && s_modal_onscreen);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // Enter shutdown charging - modal should close, shutdown charging app should launch
  battery_ui_handle_shut_down();
  cl_assert(!s_modal_onscreen && s_shutdown_charging);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // Shouldn't be able to transition out
  prv_change_state(warning_18h);
  cl_assert(!s_modal_onscreen && s_shutdown_charging);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);
}

void test_battery_ui_fsm__shutdown(void) {
  PreciseBatteryChargeState nop = prv_make_state(50, false, false),
                            charging = prv_make_state(50, true, true);

  // Shutdown while normal - enter standby
  prv_change_state(nop);
  battery_ui_handle_shut_down();
  cl_assert(!s_shutdown_charging && s_entered_standby);

  // Shutdown while charging - enter shutdown charging
  prv_change_state(charging);
  battery_ui_handle_shut_down();
  cl_assert(s_shutdown_charging);
}

void test_battery_ui_fsm__warning(void) {
  PreciseBatteryChargeState nop = prv_make_state(50, false, false);
  PreciseBatteryChargeState warning_18h =
    prv_make_state(battery_curve_get_percent_remaining(18), false, false);
  PreciseBatteryChargeState warning_12h =
    prv_make_state(battery_curve_get_percent_remaining(12), false, false);

  // Make sure warning modals don't go back up
  prv_change_state(warning_12h);
  prv_change_state(warning_18h);
  // We started at 12h warning, so only update once
  cl_assert(s_modal_onscreen);
  cl_assert_equal_i(s_modal_percent, battery_curve_get_percent_remaining(12));
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  // But we can jump around as long as we switch first
  prv_change_state(nop);
  cl_assert(!s_modal_onscreen);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  prv_change_state(warning_12h);
  cl_assert(s_modal_onscreen && s_modal_percent == battery_curve_get_percent_remaining(12));
  cl_assert_equal_i(s_led_state, StatusLedState_Off);
}

void test_battery_ui_fsm__honor_dnd(void) {
  PreciseBatteryChargeState nop = prv_make_state(50, false, false),
                            charging = prv_make_state(50, true, true),
                            warning = prv_make_state(15, false, false);
  s_dnd_on = true;
  prv_change_state(charging);
  cl_assert(s_modal_onscreen && s_modal_charging);
  cl_assert_equal_i(s_vibe_count, 0);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // With DND off, another charging event shouldn't vibe since we didn't update
  s_dnd_on = false;
  prv_change_state(charging);
  cl_assert_equal_i(s_vibe_count, 0);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // Now we should vibe
  prv_change_state(nop);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  prv_change_state(charging);
  cl_assert(s_modal_onscreen && s_modal_charging);
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // Same for warnings
  s_dnd_on = true;
  prv_change_state(warning);
  cl_assert(s_modal_onscreen && s_modal_percent);
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  s_dnd_on = false;
  prv_change_state(warning);
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  prv_change_state(nop);
  prv_change_state(warning);
  cl_assert(s_modal_onscreen && s_modal_percent);
  cl_assert_equal_i(s_vibe_count, 2);
  cl_assert_equal_i(s_led_state, StatusLedState_Off);
}

void test_battery_ui_fsm__no_vibe_complete(void) {
  PreciseBatteryChargeState charging = prv_make_state(50, true, true),
                            fully_charged = prv_make_state(100, false, true);

  cl_assert_equal_i(s_led_state, StatusLedState_Off);

  s_dnd_on = false;
  // Charging starts
  prv_change_state(charging);
  cl_assert(s_modal_onscreen && s_modal_charging);
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_Charging);

  // Charging completes
  prv_change_state(fully_charged);
  cl_assert(s_modal_onscreen && !s_modal_charging);
  cl_assert_equal_i(s_vibe_count, 1);
  cl_assert_equal_i(s_led_state, StatusLedState_FullyCharged);
}
