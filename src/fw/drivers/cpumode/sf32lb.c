/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/cpumode.h"

#include <bf0_hal.h>

#define HCPU_FREQ_LP_MHZ 48
#define HCPU_FREQ_HP_MHZ 240

void cpumode_set(CPUMode mode) {
  __disable_irq();

  if (mode == CPUMode_LowPower) {
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_TICK, RCC_CLK_TICK_HRC48);
    HAL_RCC_HCPU_ConfigHCLK(HCPU_FREQ_LP_MHZ);
  } else {
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_TICK, RCC_CLK_TICK_HXT48);
    HAL_RCC_HCPU_ConfigHCLK(HCPU_FREQ_HP_MHZ);
  }

  __enable_irq();
}