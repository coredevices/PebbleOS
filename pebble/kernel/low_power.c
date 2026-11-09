/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/analytics/analytics.h>
#include <pbl/kernel/work.h>
#include <pbl/services/runlevel.h>

#include <apps/prf/low_power.h>
#include <kernel/event_loop.h>
#include <kernel/low_power.h>
#include <kernel/ui/modals/modal_manager.h>
#include <kernel/util/standby.h>
#include <mfg/mfg_mode/mfg_factory_mode.h>
#include <process_management/app_manager.h>
#include <process_management/worker_manager.h>

static bool s_low_power_active, s_prev_low_power_active;

static void prv_low_power_launcher_task_callback(void *unused) {
  if (s_low_power_active == s_prev_low_power_active) {
    // We settled into the same state as before toggling.
    return;
  }

  if (s_low_power_active) {
    PBL_ANALYTICS_TIMER_START(low_power_time_ms);
    worker_manager_disable();
    services_set_runlevel(RunLevel_LowPower);
  } else {
    PBL_ANALYTICS_TIMER_STOP(low_power_time_ms);
    worker_manager_enable();
    services_set_runlevel(RunLevel_Normal);
  }

  s_prev_low_power_active = s_low_power_active;
}

static void prv_low_power_toggle_timer_callback(struct pbl_work *work) {
  launcher_task_add_callback(prv_low_power_launcher_task_callback, nullptr);
}

static PBL_DELAYED_WORK_DEFINE(s_toggle_timer, prv_low_power_toggle_timer_callback);

static void prv_low_power_transition(bool active) {
  s_low_power_active = active;
  // Rapid charger connection changes (aligning magnetic connector for example)
  // will cause repeated low power on/off requests. Require that a few seconds
  // elapse without further transitions before acting upon it to give us some
  // time to settle on one state or the other.
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_toggle_timer, PBL_MSEC(3000));

  // FIXME PBL-XXXXX: This should be in a shell/prf/battery_ui_fsm.c
#ifdef CONFIG_RECOVERY_FW
  if (active) {
    app_manager_launch_new_app(&(AppLaunchConfig){
      .md = prf_low_power_app_get_info(),
    });
  } else {
    // In MFG mode, disable low power mode above but don't close the app.
    if (mfg_is_mfg_mode()) {
      return;
    }
    app_manager_close_current_app(true);
  }
#endif
}

void low_power_standby(void) {
  PBL_ANALYTICS_TIMER_STOP(low_power_time_ms);
  enter_standby(RebootReasonCode_LowBattery);
}

void low_power_enter(void) {
#ifdef CONFIG_RECOVERY_FW
  if (mfg_is_mfg_mode()) {
    return;
  }
#endif
  prv_low_power_transition(true);
}

void low_power_exit(void) {
  prv_low_power_transition(false);
}

bool low_power_is_active(void) {
  return s_low_power_active;
}
