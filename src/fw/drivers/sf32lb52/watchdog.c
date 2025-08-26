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


static WDT_HandleTypeDef hwdt = {
    .Instance = hwp_wdt1,
};
static WDT_HandleTypeDef hiwdt = {
    .Instance = hwp_iwdt,
};

#define IWDT_RELOAD_DIFFTIME 5
#define WDT_RELOADER_TIMEOUT 3
#define WDT_REBOOT_TIMEOUT 50

static int s_sys_freq = 0;

void wdt_store_exception_information(void) { return; }

static HAL_StatusTypeDef wdt_set_timeout(WDT_HandleTypeDef *wdt, uint32_t reload_timeout) {
  wdt->Init.Reload = s_sys_freq * reload_timeout;

  if (hwp_iwdt == wdt->Instance)
    wdt->Init.Reload2 = s_sys_freq * IWDT_RELOAD_DIFFTIME;
  else
    wdt->Init.Reload2 = s_sys_freq * WDT_REBOOT_TIMEOUT;

  if (HAL_WDT_Init(wdt) != HAL_OK) {
    PBL_LOG(LOG_LEVEL_ERROR, "wdg set wdt timeout failed.");
    return HAL_ERROR;
  }
  __HAL_SYSCFG_Enable_WDT_REBOOT(1);
  return HAL_OK;
}

void wdt_reconfig(void) {
  HAL_WDT_Refresh(&hwdt);
  __HAL_WDT_STOP(&hwdt);

  HAL_WDT_Refresh(&hiwdt);
  wdt_set_timeout(&hiwdt, IWDT_RELOAD_DIFFTIME);
}

void WDT_IRQHandler(void) {
  static int printed;
  if (printed == 0) {
    printed++;
    if (__HAL_SYSCFG_Get_Trigger_Assert_Flag() == 0) {
      HAL_HPAON_WakeCore(CORE_ID_LCPU);
      __HAL_SYSCFG_Trigger_Assert();
    }
    wdt_reconfig();
    wdt_store_exception_information();
  }
}

void watchdog_init(void) {
  if (HAL_PMU_LXT_ENABLED())
    s_sys_freq = RC32K_FREQ;
  else
    s_sys_freq = RC10K_FREQ;

  hwdt.Init.Reload = (uint32_t)WDT_RELOADER_TIMEOUT * s_sys_freq;
  hwdt.Init.Reload2 = hwdt.Init.Reload * WDT_REBOOT_TIMEOUT;
  __HAL_WDT_STOP(&hwdt);
  __HAL_WDT_INT(&hwdt, 1);

  if (HAL_WDT_Init(&hwdt) != HAL_OK)  // wdt init
  {
    PBL_LOG(LOG_LEVEL_ERROR, "wdg set wdt timeout failed.");
    return;
  }
  __HAL_SYSCFG_Enable_WDT_REBOOT(1);

  hiwdt.Init.Reload = (uint32_t)WDT_RELOADER_TIMEOUT * s_sys_freq + IWDT_RELOAD_DIFFTIME;
  hiwdt.Init.Reload2 = s_sys_freq * IWDT_RELOAD_DIFFTIME;
  __HAL_WDT_INT(&hiwdt, 1);
  if (HAL_WDT_Init(&hiwdt) != HAL_OK) {
    PBL_LOG(LOG_LEVEL_ERROR, "wdg set wdt timeout failed.");
    return;
  }
}

void watchdog_start(void) {
  __HAL_WDT_START(&hwdt);
  __HAL_WDT_START(&hiwdt);
}

void watchdog_feed(void) {
  HAL_WDT_Refresh(&hwdt);
  HAL_WDT_Refresh(&hiwdt);
}

bool watchdog_check_reset_flag(void) { return 0; }

McuRebootReason watchdog_clear_reset_flag(void) {
  McuRebootReason mcu_reboot_reason = {
      .brown_out_reset = 0,
      .pin_reset = 0,
      .power_on_reset = 1,
      .software_reset = 0,
      .independent_watchdog_reset = 0,
      .window_watchdog_reset = 0,
      .low_power_manager_reset = 0,
  };

  return mcu_reboot_reason;
}
