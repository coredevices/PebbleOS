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

#include "FreeRTOS.h"
#include "task.h"
#include "mcu/interrupts.h"
#include "drivers/rtc.h"
#include "drivers/lptim_systick.h"
#include "system/logging.h"

#include "bf0_hal_lptim.h"
#include "bf0_hal_aon.h"

/* Use RC10K as lptim1 clock source */
#define SYSTICK_CLOCK_HZ       10000
#define SYSTICK_ONE_TICK_HZ    (SYSTICK_CLOCK_HZ / RTC_TICKS_HZ)

#if !defined(configUSE_TICKLESS_IDLE) || (configUSE_TICKLESS_IDLE != 2)
#error "lptim systick requires configUSE_TICKLESS_IDLE=2"
#endif

#ifdef SF32LB52_USE_LXT
#error "lptim systick not compatible with LXT"
#endif

static LPTIM_HandleTypeDef s_lptim1_handle = {0};
static bool s_lptim_systick_initialized = false;
static uint32_t s_last_idle_counter = 0;

void lptim_systick_init(void)
{
  HAL_LPTIM_InitDefault(&s_lptim1_handle);
  s_lptim1_handle.Instance = LPTIM1;
  // Using RC10K as LPTIM1 clock source, counter step is 100us.
  s_lptim1_handle.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  s_lptim1_handle.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  s_lptim1_handle.Init.Clock.IntSource = LPTIM_INTCLOCKSOURCE_LPCLOCK;
  s_lptim1_handle.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  HAL_LPTIM_Init(&s_lptim1_handle);

  NVIC_SetPriority(LPTIM1_IRQn, configKERNEL_INTERRUPT_PRIORITY);
  NVIC_EnableIRQ(LPTIM1_IRQn);

  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LPTIM1, AON_PIN_MODE_HIGH);    // LPPTIM1 OC wakeup
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_IRQ, AON_PIN_MODE_HIGH); // LP2HP mailbox interrupt
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_REQ, AON_PIN_MODE_HIGH); // LP2HP manual wakeup
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN10, AON_PIN_MODE_HIGH); // button PA34
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN11, AON_PIN_MODE_LOW);  // button PA35
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN12, AON_PIN_MODE_LOW);  // button PA36
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PIN13, AON_PIN_MODE_LOW);  // button PA37

  s_lptim_systick_initialized = true;
}

bool lptim_systick_is_initialized(void)
{
  return s_lptim_systick_initialized;
}

void lptim_systick_enable(void)
{
  __HAL_LPTIM_ENABLE(&s_lptim1_handle);
  __HAL_LPTIM_COUNTRST_RESET(&s_lptim1_handle);
  __HAL_LPTIM_AUTORELOAD_SET(&s_lptim1_handle, 0xFFFF);
  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, SYSTICK_ONE_TICK_HZ);
  __HAL_LPTIM_ENABLE_IT(&s_lptim1_handle, LPTIM_IT_OCIE);

  __HAL_LPTIM_START_CONTINUOUS(&s_lptim1_handle);
}

void lptim_systick_pause(void)
{
  __HAL_LPTIM_DISABLE_IT(&s_lptim1_handle, LPTIM_IT_OCIE);
  // Make sure no pending OC interrupt.
  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OC) != RESET) {
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_IT_OCIE);
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
  }
}

void lptim_systick_resume(void)
{
  uint32_t counter = LPTIM1->CNT;

  // Enable OC for the next tick.
  counter += SYSTICK_ONE_TICK_HZ;
  counter &= 0xFFFF;
  if (counter == 0xFFFF) {
    counter = 0;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
  __HAL_LPTIM_ENABLE_IT(&s_lptim1_handle, LPTIM_IT_OCIE);
}

void lptim_systick_tickless_idle(uint32_t ticks_from_now)
{
  // In tickless idle mode, use OCWE instead.
  uint32_t counter = LPTIM1->CNT;
  s_last_idle_counter = counter;

  counter += ticks_from_now * SYSTICK_ONE_TICK_HZ;
  counter &= 0xFFFF;
  if (counter == 0xFFFF) {
    counter = 0;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
  __HAL_LPTIM_ENABLE_IT(&s_lptim1_handle, LPTIM_IT_OCWE);
}

uint32_t lptim_systick_get_elapsed_ticks(void)
{
  uint32_t counter = LPTIM1->CNT;

  if (counter < s_last_idle_counter) {
    counter += 0x10000;
  }
  uint32_t counter_elapsed = counter - s_last_idle_counter;

  return counter_elapsed / SYSTICK_ONE_TICK_HZ;
}

static inline void lptim_systick_next_tick_setup(void)
{
  uint32_t counter = LPTIM1->CNT;

  counter += SYSTICK_ONE_TICK_HZ;
  counter &= 0xFFFF;
  if (counter == 0xFFFF) {
    counter = 0;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
}

void LPTIM1_IRQHandler(void)
{
  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OC) != RESET) {
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_IT_OCIE);

    extern void SysTick_Handler();
    SysTick_Handler();
    lptim_systick_next_tick_setup();
  }

  if ((__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OCWKUP) != RESET)) {
    __HAL_LPTIM_DISABLE_IT(&s_lptim1_handle, LPTIM_IT_OCWE);
    // LPTIM_IT_UEWE means all wakeup interrupt.
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_IT_UEWE);
  }
}

void AON_IRQHandler(void)
{
  NVIC_DisableIRQ(AON_IRQn);
  HAL_HPAON_CLEAR_POWER_MODE();
}
