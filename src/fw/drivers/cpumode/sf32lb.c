/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/cpumode.h"

#include <bf0_hal.h>

static uint32_t s_current_mhz = CPUMODE_FREQ_HIGH_MHZ;

static void prv_update_deep_wfi_div(uint32_t hclk_mhz) {
  // Keep deep-WFI HCLK near 4MHz regardless of the active tier.
  uint32_t div = hclk_mhz / 4;
  if (div < 1) {
    div = 1;
  }
  HAL_RCC_HCPU_SetDeepWFIDiv(div, 0, 1);
}

static bool prv_is_valid_freq(uint32_t freq_mhz) {
  return freq_mhz == CPUMODE_FREQ_LIGHT_MHZ || freq_mhz == CPUMODE_FREQ_MEDIUM_MHZ ||
         freq_mhz == CPUMODE_FREQ_HIGH_MHZ;
}

void cpumode_set_freq_mhz(uint32_t freq_mhz) {
  if (!prv_is_valid_freq(freq_mhz) || freq_mhz == s_current_mhz) {
    return;
  }

  __disable_irq();

  if (freq_mhz <= CPUMODE_FREQ_LIGHT_MHZ) {
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_TICK, RCC_CLK_TICK_HRC48);
  } else {
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_TICK, RCC_CLK_TICK_HXT48);
  }

  HAL_RCC_HCPU_ConfigHCLK(freq_mhz);
  prv_update_deep_wfi_div(freq_mhz);
  s_current_mhz = freq_mhz;

  __enable_irq();
}

uint32_t cpumode_get_freq_mhz(void) {
  return s_current_mhz;
}

void cpumode_set(CPUMode mode) {
  cpumode_set_freq_mhz(mode == CPUMode_LowPower ? CPUMODE_FREQ_LIGHT_MHZ : CPUMODE_FREQ_HIGH_MHZ);
}
