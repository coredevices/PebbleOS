/* SPDX-CopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/kernel_heap.h"
#include "memfault/components.h"
#include "drivers/imu/lsm6dso/lsm6dso.h"
#include "services/common/battery/battery_state.h"
#include "util/heap.h"
#include "services/common/analytics/analytics.h"
#include "shell/normal/watchface.h"
#include "process_management/app_install_manager.h"

int memfault_platform_get_stateofcharge(sMfltPlatformBatterySoc *soc) {
  BatteryChargeState chargestate = battery_get_charge_state();

  *soc = (sMfltPlatformBatterySoc){
      .soc = chargestate.charge_percent,
      .discharging = !chargestate.is_charging,
  };

  return 0;
}

void memfault_metrics_heartbeat_collect_data(void) {
  analytics_external_update();
}
