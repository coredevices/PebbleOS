/* SPDX-FileCopyrightText: 2026 Shashvat Prabhu */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/battery.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/battery/battery_charge_limit.h>
#include <pbl/services/battery/battery_state.h>
#include <pbl/util/testing.h>

#include <shell/prefs.h>

PBL_LOG_MODULE_DECLARE(service_battery, CONFIG_SERVICE_BATTERY_LOG_LEVEL);

#define RECHECK_INTERVAL_MS (60 * 1000)

PBL_T_STATIC bool s_limit_active;

// While the charger is held off, battery state events can be sparse (the level is stable), so
// periodically force an update to notice the level dropping below the limit. The update is
// delivered as a regular battery state event, so evaluation always happens on KernelMain.
static void prv_recheck_cb(struct pbl_work *work) {
  battery_state_force_update();
}

static PBL_DELAYED_WORK_DEFINE(s_recheck_work, prv_recheck_cb);

static void prv_schedule_recheck(void) {
  pbl_delayed_work_reschedule(&pbl_sys_workq, &s_recheck_work, PBL_MSEC(RECHECK_INTERVAL_MS));
}

static void prv_set_limit_active(bool active) {
  s_limit_active = active;
  battery_set_charge_enable(!active);
  if (active) {
    prv_schedule_recheck();
  } else {
    pbl_delayed_work_cancel(&s_recheck_work);
  }
}

void battery_charge_limit_evaluate(PreciseBatteryChargeState state) {
  const uint8_t limit = shell_prefs_get_charge_limit_pct();

  if (!s_limit_active) {
    if (limit != CHARGE_LIMIT_PCT_OFF && state.is_plugged && state.pct >= limit) {
      PBL_LOG_DBG("Charge limit: disabling charging at %d pct", state.pct);
      prv_set_limit_active(true);
    }
    return;
  }

  if (limit == CHARGE_LIMIT_PCT_OFF) {
    PBL_LOG_DBG("Charge limit: disabled, re-enabling charging");
    prv_set_limit_active(false);
  } else if (!state.is_plugged) {
    // Re-enable the charger on unplug so the next plug-in charges normally.
    PBL_LOG_DBG("Charge limit: unplugged, re-enabling charging");
    prv_set_limit_active(false);
  } else if (state.pct < limit) {
    PBL_LOG_DBG("Charge limit: resuming charging at %d pct", state.pct);
    prv_set_limit_active(false);
  } else {
    prv_schedule_recheck();
  }
}

bool battery_charge_limit_is_active(void) {
  return s_limit_active;
}
