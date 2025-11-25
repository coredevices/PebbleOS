/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/timer.h"

#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

static uint32_t prv_adjust_frequency(TIM_TypeDef *stm32_timer) {
#ifdef MICRO_FAMILY_STM32F4
  PBL_ASSERTN((RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE) != RCC_DCKCFGR_TIMPRE);
#endif

  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);

  uint32_t ppre_mask;
  uint32_t clock_freq;
  uint32_t ppre_div1_mask;
  if ((uintptr_t)stm32_timer < AHB2PERIPH_BASE) {
    clock_freq = clocks.PCLK1_Frequency;
    ppre_mask = RCC_CFGR_PPRE1;
    ppre_div1_mask = RCC_CFGR_PPRE1_DIV1;
  } else { // AHB2
    clock_freq = clocks.PCLK2_Frequency;
    ppre_mask = RCC_CFGR_PPRE1;
    ppre_div1_mask = RCC_CFGR_PPRE1_DIV1;
  }

  // From STM32F2xx Reference manual, section 5.2 (Clocks):
  // The timer clock frequencies are automatically set by hardware.
  // There are two cases:
  // 1. If the APB prescaler is 1, the timer clock frequencies are set to the
  //    same frequency as that of the APB domain to which the timers are
  //    connected.
  // 2. Otherwise, they are set to twice (×2) the frequency of the APB domain
  //    to which the timers are connected.
  if ((RCC->CFGR & ppre_mask) == ppre_div1_mask) {
    return clock_freq;
  } else {
    return clock_freq * 2;
  }
}

uint16_t timer_find_prescaler(const TimerConfig *timer, uint32_t frequency) {
  uint32_t timer_clock = prv_adjust_frequency(timer->peripheral);
  PBL_ASSERT(timer_clock >= frequency, "Timer clock frequency too low (LR %p)",
             __builtin_return_address(0));
  return (timer_clock / frequency) - 1;
}

