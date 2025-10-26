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
#include "drivers/task_watchdog.h"
#include "system/logging.h"

#include "bf0_hal_lptim.h"
#include "bf0_hal_aon.h"

/* LPRC10K frequency nearly 9 KHz, fixed to 9KHz for systick */
#define SYSTICK_CLOCK_HZ       9000
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
  // Using RC10K as LPTIM1 clock source.
  s_lptim1_handle.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  s_lptim1_handle.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  s_lptim1_handle.Init.Clock.IntSource = LPTIM_INTCLOCKSOURCE_LPCLOCK;
  s_lptim1_handle.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  HAL_LPTIM_Init(&s_lptim1_handle);

  NVIC_SetPriority(LPTIM1_IRQn, configKERNEL_INTERRUPT_PRIORITY);

  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LPTIM1, AON_PIN_MODE_HIGH);    // LPPTIM1 OC wakeup
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_IRQ, AON_PIN_MODE_HIGH); // LP2HP mailbox interrupt
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_REQ, AON_PIN_MODE_HIGH); // LP2HP manual wakeup

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

  NVIC_EnableIRQ(LPTIM1_IRQn);
}

void lptim_systick_tickless_idle(uint32_t ticks_from_now)
{
  // In tickless idle mode, use OCWE instead.
  uint32_t counter = LPTIM1->CNT;
  s_last_idle_counter = counter;

  counter += ticks_from_now * SYSTICK_ONE_TICK_HZ;
  if (counter >= 0xFFFF) {
    counter -= 0xFFFF;
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

  return (counter - s_last_idle_counter) / SYSTICK_ONE_TICK_HZ;
}

static inline void lptim_systick_next_tick_setup(void)
{
  uint32_t counter = LPTIM1->CNT;

  counter += SYSTICK_ONE_TICK_HZ;
  if (counter >= 0xFFFF) {
    counter -= 0xFFFF;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
}

void LPTIM1_IRQHandler(void)
{
  static uint32_t wdt_feed_cnt = 0U;

  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OC) != RESET) {
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_IT_OCIE);
    lptim_systick_next_tick_setup();

    // If not in tickless idle mode, call SysTick_Handler directly.
    if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OCWKUP) == RESET) {
      extern void SysTick_Handler();
      SysTick_Handler();
    }

    wdt_feed_cnt++;
    if (wdt_feed_cnt >= (RTC_TICKS_HZ * TASK_WATCHDOG_FEED_PERIOD_MS) / 1000) {
      wdt_feed_cnt = 0U;
      task_watchdog_feed();
    }
  }

  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OCWKUP) != RESET) {
    __HAL_LPTIM_DISABLE_IT(&s_lptim1_handle, LPTIM_IT_OCWE);
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_ICR_WKUPCLR);
  }
}

void AON_IRQHandler(void)
{
  NVIC_DisableIRQ(AON_IRQn);
  HAL_HPAON_CLEAR_POWER_MODE();

  uint32_t status = HAL_HPAON_GET_WSR();
  status &= ~HPSYS_AON_WSR_PIN_ALL;
  HAL_HPAON_CLEAR_WSR(status);
}
