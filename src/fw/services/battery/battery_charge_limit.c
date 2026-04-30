/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/battery/battery_charge_limit.h"

#include "drivers/battery.h"
#include "pbl/services/regular_timer.h"
#include "shell/prefs.h"
#include "system/logging.h"

#define CHARGE_LIMIT_PCT 80
#define PERIODIC_CHECK_INTERVAL_S 60

////////////////////////
// State
T_STATIC bool s_limit_active;
static RegularTimerInfo s_periodic_timer;

static void prv_periodic_timer_cb(void *data) {
  BatteryChargeState charge = battery_get_charge_state();
  PreciseBatteryChargeState state = {
    .pct = charge.charge_percent,
    .is_plugged = charge.is_plugged,
    .is_charging = charge.is_charging,
  };
  battery_charge_limit_evaluate(state);
}

void battery_charge_limit_init(void) {
  s_periodic_timer.cb = prv_periodic_timer_cb;
  regular_timer_add_multisecond_callback(&s_periodic_timer, PERIODIC_CHECK_INTERVAL_S);
}

void battery_charge_limit_evaluate(PreciseBatteryChargeState state) {
  if (!shell_prefs_get_charge_limit_enabled()) {
    if (s_limit_active) {
      battery_set_charge_enable(true);
      s_limit_active = false;
      PBL_LOG_INFO("Charge limit: disabled, re-enabling charging");
    }
    return;
  }

  if (!state.is_plugged) {
    s_limit_active = false;
    return;
  }

  if (state.pct >= CHARGE_LIMIT_PCT && !s_limit_active) {
    battery_set_charge_enable(false);
    s_limit_active = true;
    PBL_LOG_INFO("Charge limit: disabling charging at %d pct", state.pct);
  } else if (state.pct < CHARGE_LIMIT_PCT && s_limit_active) {
    battery_set_charge_enable(true);
    s_limit_active = false;
    PBL_LOG_INFO("Charge limit: resuming charging at %d pct", state.pct);
  }
}

bool battery_charge_limit_is_active(void) {
  return s_limit_active;
}
