/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "setup.h"

#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"

#ifdef MICRO_FAMILY_STM32F4
#include <stm32f4xx.h>
#endif

void enable_mcu_debugging(void) {
#if !defined(RELEASE) && !defined(MICRO_FAMILY_NRF52) && !defined(MICRO_FAMILY_SF32LB52) && !defined(MICRO_FAMILY_QEMU)
  DBGMCU_Config(DBGMCU_SLEEP | DBGMCU_STOP, ENABLE);
  // Stop RTC, IWDG & TIM2 during debugging
  // Note: TIM2 is used by the task watchdog
  DBGMCU_APB1PeriphConfig(DBGMCU_RTC_STOP | DBGMCU_TIM2_STOP | DBGMCU_IWDG_STOP,
                          ENABLE);
#endif
}

void disable_mcu_debugging(void) {
#if !defined(RELEASE) && !defined(MICRO_FAMILY_NRF52) && !defined(MICRO_FAMILY_SF32LB52) && !defined(MICRO_FAMILY_QEMU)
  DBGMCU->CR = 0;
  DBGMCU->APB1FZ = 0;
  DBGMCU->APB2FZ = 0;
#endif
}

void command_low_power_debug(char *cmd) {
  bool low_power_debug_on = (strcmp(cmd, "on") == 0);

#ifdef MICRO_FAMILY_STM32F4
  sleep_mode_enable(!low_power_debug_on);
#endif

  if (low_power_debug_on) {
    enable_mcu_debugging();
  } else {
    disable_mcu_debugging();
  }
}
