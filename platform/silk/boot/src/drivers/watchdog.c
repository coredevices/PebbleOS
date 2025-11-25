/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"

#include "stm32f4xx_dbgmcu.h"
#include "stm32f4xx_iwdg.h"
#include "stm32f4xx_rcc.h"

void watchdog_init(void) {
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

  IWDG_SetPrescaler(IWDG_Prescaler_64); // ~8 seconds
  IWDG_SetReload(0xfff);

  IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);

  DBGMCU_APB1PeriphConfig(DBGMCU_IWDG_STOP, ENABLE);
}

void watchdog_start(void) {
  IWDG_Enable();
  IWDG_ReloadCounter();
}

bool watchdog_check_reset_flag(void) {
  return RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET;
}
