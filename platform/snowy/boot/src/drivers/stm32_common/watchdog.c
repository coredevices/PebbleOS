/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"

#include "util/bitset.h"
#include "system/logging.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_dbgmcu.h"
#include "stm32f2xx_iwdg.h"
#include "stm32f2xx_rcc.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_dbgmcu.h"
#include "stm32f4xx_iwdg.h"
#include "stm32f4xx_rcc.h"
#endif

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

void watchdog_clear_reset_flag(void) {
  RCC_ClearFlag();
}
