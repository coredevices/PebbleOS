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

#include "services/common/battery/battery_monitor.h"

#include "board/board.h"
#include "kernel/low_power.h"
#include "kernel/util/standby.h"
#include "services/common/firmware_update.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/ratio.h"

#include <stdint.h>

#define BATT_LOG_COLOR LOG_COLOR_YELLOW

// State machine stuff

typedef void (*Action)(void);

typedef struct PowerState {
  Action enter;
  Action exit;
} PowerState;

typedef enum {
  PowerStateGood,
  PowerStateLowPower,
  PowerStateCritical,
  PowerStateStandby
} PowerStateID;

static void prv_enter_lpm(void);
static void prv_exit_lpm(void);
static void prv_begin_standby_timer(void);
static void prv_enter_standby(void);
static void prv_exit_critical(void);

static const PowerState power_states[] = {
  [PowerStateGood] = { 0 },
  [PowerStateLowPower] = { .enter = prv_enter_lpm, .exit = prv_exit_lpm },
  [PowerStateCritical] = { .enter = prv_begin_standby_timer, .exit = prv_exit_critical },
  [PowerStateStandby] = { .enter = prv_enter_standby }
};

////////////////////////
// Business logic
static TimerID s_standby_timer_id = TIMER_INVALID_ID;
T_STATIC PowerStateID s_power_state;
static bool s_low_on_first_run;
static bool s_first_run;

static void prv_transition(PowerStateID next_state) {
  if (next_state == s_power_state) {
    return;
  }
  PowerStateID old_state = s_power_state;
  s_power_state = next_state;
  if (power_states[old_state].exit) {
    power_states[old_state].exit();
  }
  if (power_states[next_state].enter) {
    power_states[next_state].enter();
  }
}

static void prv_enter_lpm(void) {
#ifndef BATTERY_DEBUG
  if (!firmware_update_is_in_progress()) {
    low_power_enter();
  }
#endif
  PBL_LOG_COLOR(LOG_LEVEL_INFO, BATT_LOG_COLOR, "Battery low: enter low power mode");
}

static void prv_resume_normal_operation(void) {
  low_power_exit();
  PBL_LOG_COLOR(LOG_LEVEL_INFO, BATT_LOG_COLOR, "Battery good: resume normal operation");
}

static void prv_exit_critical(void) {
  // Checking the state here is a bit of a hack because the state machine does not have proper
  // transition actions, only entry/exit actions.
  // We check that the state is PowerStateGood because the state machine does not transition through
  // all states in between the new and old states in a transition.
  if (s_power_state == PowerStateGood) {
    prv_resume_normal_operation();
  }
}

static void prv_exit_lpm(void) {
  // Checking the state here is a bit of a hack because the state machine does not have proper
  // transition actions, only entry/exit actions
  if (s_power_state == PowerStateGood) {
    prv_resume_normal_operation();
  }
}

static void prv_standby_timer_callback(void* data) {
  // FIXME This is so broken: battery_state_force_update schedules a new timer callback to execute
  // immediately, which then pends a background task callback to perform the update, so this will
  // never update before we check the power_state.
  battery_state_force_update();
  if (s_power_state == PowerStateCritical) {
    // Still critical after timeout, transition to standby
    prv_transition(PowerStateStandby);
  }
}

static void prv_begin_standby_timer(void) {
  PBL_LOG_COLOR(LOG_LEVEL_INFO, BATT_LOG_COLOR, "Battery critical: begin standby timer");
  // If the watch was already running, give them 30s, otherwise just 2s.
  uint32_t standby_timeout = (s_first_run) ? 2000: 30000;
  new_timer_start(s_standby_timer_id, standby_timeout,
      prv_standby_timer_callback, NULL, 0 /*flags*/);
}

static void system_task_handle_battery_critical(void* data) {
  PBL_LOG_COLOR(LOG_LEVEL_INFO, BATT_LOG_COLOR, "Battery critical: go to standby mode");
  if (low_power_is_active()) {
    low_power_standby();
  } else {
    enter_standby(RebootReasonCode_LowBattery);
  }
}

static void prv_enter_standby(void) {
  system_task_add_callback(system_task_handle_battery_critical, NULL);
}

static void prv_log_battery_state(PreciseBatteryChargeState state) {
  const uint16_t k_min_percent_diff = 5;
  const uint16_t percent = ratio32_to_percent(state.charge_percent);

  union LoggingBattState{
    struct {
      uint16_t is_charging:1;
      uint16_t is_plugged:1;
      uint16_t percent:14;
    };
    uint16_t all;
  };
  static union LoggingBattState s_prev_batt_state;

  union LoggingBattState new_batt_state = {
    .percent = percent / k_min_percent_diff,
    .is_charging = state.is_charging,
    .is_plugged = state.is_plugged,
  };

  if ((percent < BOARD_CONFIG_POWER.low_power_threshold) ||
      (s_prev_batt_state.all != new_batt_state.all) ||
      s_first_run) {
        s_prev_batt_state.all = new_batt_state.all;
        PBL_LOG_COLOR(LOG_LEVEL_INFO, BATT_LOG_COLOR, "Percent: %d Charging: %d Plugged: %d",
                percent, state.is_charging, state.is_plugged);
      }
}

void battery_monitor_handle_state_change_event(PreciseBatteryChargeState state) {
  // Update Critical/Low Power Mode

  // Standby behaviour, as gleaned from the previous implementation:
  //  Once the battery voltage falls below exactly 0%, the standby lockout is displayed.
  //  If the USB cable is disconnected, the standby timer starts. This standby delay is 2s
  //    (if at first start), otherwise it is 30s (if the watch was already running).
  //  The shutdown can be averted if the watch is plugged in before the timer expires.
  //  Similarly, if the battery voltage has rebounded when the timer expires, the shutdown
  //    will not occur.

  bool critical = (state.charge_percent == 0) && !state.is_charging;

#ifndef RECOVERY_FW
  const uint32_t LOW_POWER_PERCENT = ratio32_from_percent(BOARD_CONFIG_POWER.low_power_threshold);

  bool low_power = !state.is_charging && (state.charge_percent <= LOW_POWER_PERCENT);
  s_low_on_first_run = s_low_on_first_run || (low_power && s_first_run);
#else
  const uint32_t PRF_LOW_POWER_THRESHOLD_PERCENT = ratio32_from_percent(5);

  // We want to keep the LPM UI up until we've hit 10% regardless of charging
  bool low_power = state.charge_percent < PRF_LOW_POWER_THRESHOLD_PERCENT;
  s_low_on_first_run = false;
#endif

  PowerStateID new_state;

  if (critical || s_low_on_first_run) {
    new_state = PowerStateCritical;
  } else if (low_power) {
    new_state = PowerStateLowPower;
  } else {
    new_state = PowerStateGood;
  }

  // All state transitions are valid in this state machine.
  prv_transition(new_state);

  prv_log_battery_state(state);

  s_first_run = false;
}

void battery_monitor_init(void) {
  s_standby_timer_id = new_timer_create();
  s_power_state = PowerStateGood;
  s_low_on_first_run = false;
  s_first_run = true;

  // Initialize driver interface
  battery_state_init();
}

bool battery_monitor_critical_lockout(void) {
  // critical or low on first run
  return s_power_state == PowerStateCritical;
}

TimerID battery_monitor_get_standby_timer_id(void) {
  return s_standby_timer_id;
}
