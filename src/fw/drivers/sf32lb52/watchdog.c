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

#include "util/bitset.h"
#include "system/logging.h"
#include <inttypes.h>
#include "bf0_hal.h"

#define WDT_RELOADER_TIMEOUT 3
#define WDT_REBOOT_TIMEOUT 8

static WDT_HandleTypeDef hwdt = {
    .Instance = hwp_wdt1,
};

static int s_sys_freq = 0;
static int s_irq_handle = 0;

void wdt_reconfig(void) {
  HAL_WDT_Refresh(&hwdt);
  __HAL_WDT_STOP(&hwdt);
}

void watchdog_init(void) {
  HAL_StatusTypeDef status;

  if (HAL_PMU_LXT_ENABLED()) {
    s_sys_freq = RC32K_FREQ;
  } else {
    s_sys_freq = RC10K_FREQ;
  }

  hwdt.Init.Reload = (uint32_t)WDT_RELOADER_TIMEOUT * s_sys_freq;
  hwdt.Init.Reload2 = (uint32_t)WDT_REBOOT_TIMEOUT * s_sys_freq;
  __HAL_WDT_STOP(&hwdt);
  __HAL_WDT_INT(&hwdt, 1);

  status = HAL_WDT_Init(&hwdt);
  PBL_ASSERTN(status == HAL_OK);

  __HAL_SYSCFG_Enable_WDT_REBOOT(1);
  s_irq_handle = 0;
  PBL_LOG(LOG_LEVEL_DEBUG, "watchdog: Initialied");
  return;
}

void watchdog_start(void) { __HAL_WDT_START(&hwdt); }

void watchdog_feed(void) { 
  HAL_WDT_Refresh(&hwdt);
}

bool watchdog_check_reset_flag(void) { return (HAL_PMU_GET_WSR() & PMUC_WSR_WDT1) != 0; }

McuRebootReason watchdog_clear_reset_flag(void) {
  uint32_t wsr = HAL_PMU_GET_WSR();
  uint32_t boot = SystemPowerOnModeGet();
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
