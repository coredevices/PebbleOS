/*
 * Copyright 2025 Core Devices LLC
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

#include "drivers/watchdog.h"
#include "system/logging.h"

#include "bf0_hal.h"

#define WDT_TIMEOUT_S 10U

static WDT_HandleTypeDef hwdt = {
    .Instance = hwp_wdt1,
};

void watchdog_init(void) {
  HAL_StatusTypeDef status;

  // Assume RC10K is used as WDT clock source.
  hwdt.Init.Reload = RC10K_FREQ * WDT_TIMEOUT_S;

  __HAL_WDT_STOP(&hwdt);
  __HAL_WDT_INT(&hwdt, 0);
  __HAL_WDT_RELOAD_COUNTER(&hwdt);

  __HAL_SYSCFG_Enable_WDT_REBOOT(1);

  PBL_LOG(LOG_LEVEL_DEBUG, "Initializing WDT with timeout %u seconds", WDT_TIMEOUT_S);
}

void watchdog_start(void) {
  __HAL_WDT_START(&hwdt);
}

void watchdog_feed(void) {
  __HAL_WDT_START(&hwdt);
}

bool watchdog_check_reset_flag(void) {
  return (HAL_PMU_GET_WSR() & PMUC_WSR_WDT1) != 0;
}

McuRebootReason watchdog_clear_reset_flag(void) {
  uint32_t wsr = HAL_PMU_GET_WSR();
  pm_power_on_mode_t boot = SystemPowerOnModeGet();
  HAL_PMU_CLEAR_WSR(0xFFFFFFFF);

  McuRebootReason mcu_reboot_reason = {
      .brown_out_reset = 0,
      .pin_reset = (((wsr & PMUC_WSR_PIN0) != 0) || ((wsr & PMUC_WSR_PIN1) != 0)),
      .power_on_reset = (boot & PM_COLD_BOOT) != 0,
      .software_reset = (boot & PM_REBOOT_BOOT) != 0,
      .independent_watchdog_reset = 0,
      .window_watchdog_reset = (wsr & PMUC_WSR_WDT1) != 0,
      .low_power_manager_reset = 0,
  };

  return mcu_reboot_reason;
}
