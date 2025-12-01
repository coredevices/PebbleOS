/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "FreeRTOS.h"
#include "task.h"
#include "mcu/interrupts.h"
#include "drivers/rtc.h"
#include "drivers/lptim_systick.h"
#include "drivers/task_watchdog.h"
#include "services/common/new_timer/new_timer.h"
#include "system/logging.h"

#include "bf0_hal_lptim.h"
#include "bf0_hal_aon.h"

#if !defined(configUSE_TICKLESS_IDLE) || (configUSE_TICKLESS_IDLE != 2)
#error "lptim systick requires configUSE_TICKLESS_IDLE=2"
#endif

#ifdef SF32LB52_USE_LXT
#error "lptim systick not compatible with LXT"
#endif

// RC calibration clock cyles
#define LXT_LP_CYCLE 200U
// Calibration period (ms)
#define CAL_PERIOD_MS 60000

#define LPTIM_COUNT_MAX 0xFFFFU

static LPTIM_HandleTypeDef s_lptim1_handle = {0};
static bool s_lptim_systick_initialized = false;
static uint32_t s_last_idle_counter = 0;
static TimerID s_cal_timer;
static uint16_t s_one_tick_hz;

static void prv_cal_timer_cb(void* data) {
  uint32_t ref_cycle;

  HAL_RC_CAL_update_reference_cycle_on_48M(LXT_LP_CYCLE);

  ref_cycle = HAL_Get_backup(RTC_BACKUP_LPCYCLE);
  s_one_tick_hz = ((48000000ULL * LXT_LP_CYCLE) / ref_cycle) / RTC_TICKS_HZ;

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, s_one_tick_hz);
}

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
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_GPIO1, AON_PIN_MODE_HIGH);

  prv_cal_timer_cb(NULL);

  s_lptim_systick_initialized = true;
}

void lptim_calibrate_init(void)
{
  bool ret;

  s_cal_timer = new_timer_create();
  PBL_ASSERTN(s_cal_timer != TIMER_INVALID_ID);

  ret = new_timer_start(s_cal_timer, CAL_PERIOD_MS, prv_cal_timer_cb, NULL,
                        TIMER_START_FLAG_REPEATING);
  PBL_ASSERTN(ret);
}

bool lptim_systick_is_initialized(void)
{
  return s_lptim_systick_initialized;
}

void lptim_systick_enable(void)
{
  __HAL_LPTIM_ENABLE(&s_lptim1_handle);
  __HAL_LPTIM_COUNTRST_RESET(&s_lptim1_handle);
  __HAL_LPTIM_AUTORELOAD_SET(&s_lptim1_handle, LPTIM_COUNT_MAX);
  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, s_one_tick_hz);
  __HAL_LPTIM_ENABLE_IT(&s_lptim1_handle, LPTIM_IT_OCIE);

  __HAL_LPTIM_START_CONTINUOUS(&s_lptim1_handle);

  NVIC_EnableIRQ(LPTIM1_IRQn);
}

void lptim_systick_tickless_idle(uint32_t ticks_from_now)
{
  // In tickless idle mode, use OCWE instead.
  uint32_t counter = LPTIM1->CNT;
  s_last_idle_counter = counter;

  counter += ticks_from_now * s_one_tick_hz;
  if (counter >= LPTIM_COUNT_MAX) {
    counter -= LPTIM_COUNT_MAX;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
  __HAL_LPTIM_ENABLE_IT(&s_lptim1_handle, LPTIM_IT_OCWE);
}

uint32_t lptim_systick_get_elapsed_ticks(void)
{
  uint32_t counter = LPTIM1->CNT;

  if (counter < s_last_idle_counter) {
    counter += (LPTIM_COUNT_MAX + 1);
  }

  return (counter - s_last_idle_counter) / s_one_tick_hz;
}

static inline void lptim_systick_next_tick_setup(void)
{
  uint32_t counter = LPTIM1->CNT;

  counter += s_one_tick_hz;
  if (counter >= LPTIM_COUNT_MAX) {
    counter -= LPTIM_COUNT_MAX;
  }

  __HAL_LPTIM_COMPARE_SET(&s_lptim1_handle, counter);
}

void LPTIM1_IRQHandler(void)
{
  static uint32_t wdt_last_counter = 0U;
  static uint32_t wdt_feed_counter = 0U;

  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OC) != RESET) {
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_IT_OCIE);
    lptim_systick_next_tick_setup();

    // If not in tickless idle mode, call SysTick_Handler directly.
    if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OCWKUP) == RESET) {
      extern void SysTick_Handler();
      SysTick_Handler();

      uint32_t current_counter = LPTIM1->CNT;
      if (current_counter < wdt_last_counter) {
        current_counter += (LPTIM_COUNT_MAX + 1);
      }
      wdt_feed_counter += (current_counter - wdt_last_counter);
      wdt_last_counter = current_counter & LPTIM_COUNT_MAX;
      if (wdt_feed_counter >= (TASK_WATCHDOG_FEED_PERIOD_MS * s_one_tick_hz)) {
        wdt_feed_counter = 0U;
        task_watchdog_feed();
      }
    }
  }

  if (__HAL_LPTIM_GET_FLAG(&s_lptim1_handle, LPTIM_FLAG_OCWKUP) != RESET) {
    __HAL_LPTIM_DISABLE_IT(&s_lptim1_handle, LPTIM_IT_OCWE);
    __HAL_LPTIM_CLEAR_FLAG(&s_lptim1_handle, LPTIM_ICR_WKUPCLR);

    // Force a watchdog refresh immediately after wakeup. The LPTIM SysTick requires
    // time to restart; if the system re-enters Stop mode during this latency, a watchdog
    // timeout may occur.
    task_watchdog_bit_set_all();
    task_watchdog_feed();
    // refresh wdt feed counter
    wdt_feed_counter = 0;
    wdt_last_counter = LPTIM1->CNT;
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
