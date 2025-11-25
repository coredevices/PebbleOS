/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"

#include "util/bitset.h"
#include "system/logging.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <inttypes.h>

void watchdog_init(void) {
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

  IWDG_SetPrescaler(IWDG_Prescaler_64); // ~8 seconds
  IWDG_SetReload(0xfff);

  IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);

  DBGMCU_APB1PeriphConfig(DBGMCU_IWDG_STOP, ENABLE);
}

void watchdog_start(void) {
  IWDG_Enable();
  watchdog_feed();
}

// This behaves differently from the bootloader and the firmware.
void watchdog_feed(void) {
  IWDG_ReloadCounter();
}

bool watchdog_check_reset_flag(void) {
  return RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET;
}

McuRebootReason watchdog_clear_reset_flag(void) {
  McuRebootReason mcu_reboot_reason = {
    .brown_out_reset = (RCC_GetFlagStatus(RCC_FLAG_BORRST) != RESET),
    .pin_reset = (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET),
    .power_on_reset = (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET),
    .software_reset = (RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET),
    .independent_watchdog_reset = (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET),
    .window_watchdog_reset = (RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET),
    .low_power_manager_reset = (RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
  };

  RCC_ClearFlag();

  return mcu_reboot_reason;
}
