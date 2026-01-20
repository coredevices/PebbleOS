/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdio.h>

#include "console/prompt.h"
#include "drivers/flash.h"
#include "drivers/lptim_systick.h"
#include "drivers/mcu.h"
#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"
#include "kernel/util/stop.h"
#include "kernel/util/wfi.h"
#include "os/tick.h"
#include "util/math.h"

#define SF32LB52_COMPATIBLE
#include <mcu.h>

#include <ipc_queue.h>

#include "FreeRTOS.h"
#include "task.h"

static uint64_t s_analytics_device_sleep_cpu_cycles = 0;
static RtcTicks s_analytics_device_stop_ticks = 0;

static uint32_t s_last_ticks_elapsed_in_stop = 0;
static uint32_t s_last_ticks_commanded_in_stop = 0;

//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks EARLY_WAKEUP_TICKS = 4;
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks MIN_STOP_TICKS = 8;
// 1 second ticks so that we only wake up once every regular timer interval.
static const RtcTicks MAX_STOP_TICKS = RTC_TICKS_HZ;

static uint32_t s_iser_bak[16];

static void prv_save_iser(void)
{
    uint32_t i;
    for (i = 0; i < 16; i++)
    {
        s_iser_bak[i] = NVIC->ISER[i];
        NVIC->ICER[i] = 0xFFFFFFFF;
        __DSB();
        __ISB();
    }
}

static void prv_restore_iser(void)
{
    uint32_t i;
    for (i = 0; i < 16; i++)
    {
        __COMPILER_BARRIER();
        NVIC->ISER[i] = s_iser_bak[i];
        __COMPILER_BARRIER();
    }
}

static void prv_enter_deepslep(void) {
  uint32_t dll1_freq;
  uint32_t dll2_freq;
  int clk_src;

  prv_save_iser();

  flash_power_down_for_stop_mode();

  NVIC_EnableIRQ(AON_IRQn);
  NVIC_EnableIRQ(LPTIM1_IRQn);

  clk_src = HAL_RCC_HCPU_GetClockSrc(RCC_CLK_MOD_SYS);
  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HRC48);
  dll1_freq = HAL_RCC_HCPU_GetDLL1Freq();
  dll2_freq = HAL_RCC_HCPU_GetDLL2Freq();
  
  HAL_RCC_HCPU_DisableDLL1();
  HAL_RCC_HCPU_DisableDLL2();

  HAL_HPAON_DISABLE_PAD();
  HAL_HPAON_DISABLE_VHP();
  
  HAL_HPAON_CLEAR_HP_ACTIVE();
  HAL_HPAON_SET_POWER_MODE(AON_PMR_DEEP_SLEEP);

  __WFI();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  
  HAL_HPAON_ENABLE_PAD();
  HAL_HPAON_ENABLE_VHP();

  HAL_HPAON_SET_HP_ACTIVE();
  HAL_HPAON_CLEAR_POWER_MODE();

  // Wait for HXT48 to be ready
  if (dll1_freq != 0) {
    while (0 == (hwp_hpsys_aon->ACR & HPSYS_AON_ACR_HXT48_RDY)) {
      __NOP();
    }
  }

  // Switch back to original clock source
  HAL_RCC_HCPU_EnableDLL1(dll1_freq);
  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, clk_src);
  HAL_RCC_HCPU_EnableDLL2(dll2_freq);
  HAL_Delay_us(0);

  prv_restore_iser();
}

void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime ) {
  if (!sleep_mode_is_allowed() || !ipc_queue_check_idle()) {
    // To avoid LCPU enter incorrect state, make sure ipc queue is empty before enter stop mode.
    return;
  }

  __disable_irq();

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    if (xExpectedIdleTime < MIN_STOP_TICKS || !stop_mode_is_allowed()) {
      uint32_t counter_start = LPTIM1->CNT;
  
      __DSB();

      __WFI();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();
      __NOP();

      __ISB();

      uint32_t counter_stop = LPTIM1->CNT;
      if (counter_stop < counter_start) {
        counter_stop += 0x10000;
      }
      uint32_t counter_elapsed = counter_stop - counter_start;

      // Calculate elapsed ticks and step FreeRTOS tick count
      uint32_t freq = lptim_systick_get_freq_hz();
      uint32_t ticks_elapsed = (counter_elapsed * RTC_TICKS_HZ) / freq;
      if (ticks_elapsed > 0) {
        // Cap to xExpectedIdleTime to avoid FreeRTOS assertion
        if (ticks_elapsed > xExpectedIdleTime) {
          ticks_elapsed = xExpectedIdleTime;
        }
        vTaskStepTick(ticks_elapsed);
        // Clear pending LPTIM interrupt and set up next tick to avoid double-counting
        lptim_systick_sync_after_wfi();
      }

      s_analytics_device_sleep_cpu_cycles += ticks_elapsed;
    } else {
      const RtcTicks stop_duration = MIN(xExpectedIdleTime - EARLY_WAKEUP_TICKS, MAX_STOP_TICKS);

      // Go into stop mode until the wakeup_tick.
      s_last_ticks_commanded_in_stop = stop_duration;

      lptim_systick_tickless_idle((uint32_t)stop_duration);

      prv_enter_deepslep();

      lptim_systick_tickless_exit();

      uint32_t ticks_elapsed = lptim_systick_get_elapsed_ticks();

      // Cap ticks_elapsed to xExpectedIdleTime to avoid FreeRTOS assertion failure
      // in vTaskStepTick() when we oversleep due to wake-up latency or RC oscillator drift
      if (ticks_elapsed > xExpectedIdleTime) {
        ticks_elapsed = xExpectedIdleTime;
      }

      s_last_ticks_elapsed_in_stop = ticks_elapsed;
      vTaskStepTick(ticks_elapsed);

      // Update the task watchdog every time we come out of STOP mode (which is
      // at least once/second) since the timer peripheral will not have been
      // incremented. Set all watchdog bits first since the LPTIM ISR that would
      // normally do this hasn't run yet (interrupts are still globally disabled).
      task_watchdog_bit_set_all();
      task_watchdog_step_elapsed_time_ms((ticks_elapsed * 1000) / RTC_TICKS_HZ);

      s_analytics_device_stop_ticks += ticks_elapsed;
    }
  }

  __enable_irq();
}

bool vPortEnableTimer() {
  lptim_systick_enable();
  return true;
}

// CPU analytics
///////////////////////////////////////////////////////////

static uint32_t s_last_ticks = 0;
void dump_current_runtime_stats(void) {
  uint32_t stop_ms = ticks_to_milliseconds(s_analytics_device_stop_ticks);
  uint32_t sleep_ms = mcu_cycles_to_milliseconds(s_analytics_device_sleep_cpu_cycles);

  uint32_t now_ticks = rtc_get_ticks();
  uint32_t running_ms =
      ticks_to_milliseconds(now_ticks - s_last_ticks) - stop_ms - sleep_ms;
  s_last_ticks = now_ticks;
  uint32_t tot_time = running_ms + sleep_ms + stop_ms;

  char buf[160];
  snprintf(buf, sizeof(buf), "Run:   %"PRIu32" ms (%"PRIu32" %%)",
           running_ms, (running_ms * 100) / tot_time);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Sleep: %"PRIu32" ms (%"PRIu32" %%)",
           sleep_ms, (sleep_ms * 100) / tot_time);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Stop:  %"PRIu32" ms (%"PRIu32" %%)",
           stop_ms, (stop_ms * 100) / tot_time);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Tot:   %"PRIu32" ms", tot_time);
  prompt_send_response(buf);
  
  uint32_t rtc_ticks = rtc_get_ticks();
  uint32_t rtos_ticks = xTaskGetTickCount();
  snprintf(buf, sizeof(buf), "RTC ticks: %"PRIu32", RTOS ticks: %"PRIu32 ", last ticks stopped: %"PRIu32 " / %"PRIu32,
                             rtc_ticks, rtos_ticks, s_last_ticks_elapsed_in_stop, s_last_ticks_commanded_in_stop);
  prompt_send_response(buf);
}
