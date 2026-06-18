/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <inttypes.h>

#include "console/dbgserial.h"
#include "console/dbgserial_input.h"
#include "drivers/flash.h"
#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"
#include "console/prompt.h"
#include "kernel/util/stop.h"
#include "kernel/util/wfi.h"
#include "pbl/services/analytics/analytics.h"
#include "util/math.h"

#include <cmsis_core.h>

#include <hal/nrf_nvmc.h>

#include "FreeRTOS.h"
#include "task.h"

static RtcTicks s_analytics_sleep_ticks = 0;
static RtcTicks s_analytics_stop_ticks = 0;

static uint32_t s_last_ticks_elapsed_in_stop = 0;
static uint32_t s_last_ticks_commanded_in_stop = 0;
static uint32_t s_ticks_corrected = 0;

//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks EARLY_WAKEUP_TICKS = 2;
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks MIN_STOP_TICKS = 5;

// 1 second ticks so that we only wake up once every regular timer interval.
static const RtcTicks MAX_STOP_TICKS = RTC_TICKS_HZ;

extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime ) {
  if (!rtc_alarm_is_initialized() || !sleep_mode_is_allowed()) {
    // the RTC is not yet initialized to the point where it can wake us from sleep or sleep/stop
    // is disabled. Just returning will cause a busy loop where the caller thought we slept for
    // 0 ticks and will reevaluate what to do next (probably just try again).
    return;
  }

  // Note: all tasks are suspended at this point, but we can still be interrupted
  // so the critical section is necessary. taskENTER_CRITICAL() is not used here
  // as that method would mask interrupts that should exit the low-power mode.
  // The __disable_irq() function sets the PRIMASK bit which globally prevents
  // interrupt execution while still allowing interrupts to wake the processor
  // from WFI.
  // Conversely, taskEnter_CRITICAL() sets the BASEPRI register, which masks
  // interrupts with priorities lower than configMAX_SYSCALL_INTERRUPT_PRIORITY
  // from executing and from waking the processor.
  // See: http://infocenter.arm.com/help/topic/com.arm.doc.dui0552a/BABGGICD.html#BGBHDHAI
  __disable_irq();

  // We're going to sleep, so turn off the caches (they consume quiescent
  // power).  It's more efficient to have them on when we're awake, but for
  // now, they gotta go.  This holds true even if we're not going to sleep
  // long enough to trigger stop mode.
  NRF_NVMC->ICACHECNF &= ~NVMC_ICACHECNF_CACHEEN_Msk;

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    if (xExpectedIdleTime < MIN_STOP_TICKS || !stop_mode_is_allowed()) {
      RtcTicks sleep_start_ticks = rtc_get_ticks();

      __DSB();  // Drain any pending memory writes before entering sleep.
      do_wfi();  // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
      __ISB();  // Let the pipeline catch up (force the WFI to activate before moving on).

      s_analytics_sleep_ticks += rtc_get_ticks() - sleep_start_ticks;
    } else {
      const RtcTicks stop_duration = MIN(xExpectedIdleTime - EARLY_WAKEUP_TICKS, MAX_STOP_TICKS);

      // Go into stop mode until the wakeup_tick.
      s_last_ticks_commanded_in_stop = stop_duration;

      dbgserial_enable_rx_exti();
      dbgserial_disable_rx_dma_before_stop();
      flash_power_down_for_stop_mode();

      rtc_alarm_set(stop_duration);
      rtc_systick_pause();

      __DSB(); // Drain any pending memory writes before entering sleep.
      do_wfi(); // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
      __ISB(); // Let the pipeline catch up (force the WFI to activate before moving on).

      rtc_systick_resume();

      flash_power_up_after_stop_mode();
      dbgserial_enable_rx_dma_after_stop();

      RtcTicks ticks_elapsed = rtc_alarm_get_elapsed_ticks();

      s_last_ticks_elapsed_in_stop = ticks_elapsed;
      vTaskStepTick(ticks_elapsed);

      // Update the task watchdog every time we come out of STOP mode (which is
      // at least once/second) since the timer peripheral will not have been
      // incremented
      task_watchdog_step_elapsed_time_ms((ticks_elapsed * 1000) / RTC_TICKS_HZ);

      s_analytics_stop_ticks += ticks_elapsed;
    }
  }

  NRF_NVMC->ICACHECNF |= NVMC_ICACHECNF_CACHEEN_Msk;

  __enable_irq();
}

// Called from the SysTick handler ISR to adjust ticks for situations where the CPU might
// occasionally fall behind and miss some tick interrupts (like when running under emulation).
bool vPortCorrectTicks(void) {
  static uint8_t s_check_counter = 0;
  static int64_t s_rtc_ticks_to_rtos_ticks = 0;

  if (++s_check_counter < 10) {
    // Just check occasionally so we don't incur the overhead of reading the RTC on every
    // systick
    return false;
  }
  s_check_counter = 0;

  // Compute what ticks should be based on the real time clock.
  time_t seconds;
  uint16_t milliseconds;
  rtc_get_time_ms(&seconds, &milliseconds);
  int64_t rtc_ticks = ((((int64_t)seconds * 1000) + milliseconds) * RTC_TICKS_HZ) / 1000;
  uint32_t target_rtos_ticks = rtc_ticks + s_rtc_ticks_to_rtos_ticks;
  uint32_t act_ticks = xTaskGetTickCountFromISR();

  if (act_ticks > target_rtos_ticks + 100 || act_ticks < target_rtos_ticks - 100) {
    // If we are too far out of range of the target ticks, just reset our offsets. This could
    // be caused either by the RTC time being changed or by staying in the debugger too long
    s_rtc_ticks_to_rtos_ticks = (int64_t)act_ticks - rtc_ticks;
    return false;
  } else if (act_ticks >= target_rtos_ticks) {
    // No correction needed
    return false;
  }

  // Let's advance the RTOS ticks until we catch up
  bool need_context_switch = false;
  while (act_ticks < target_rtos_ticks) {
    /* Increment the RTOS ticks. */
    need_context_switch |= (xTaskIncrementTick() != 0);
    act_ticks++;
    s_ticks_corrected++;
  }
  return need_context_switch;
}

bool vPortEnableTimer() {
  rtc_enable_synthetic_systick();
  return true;
}

// CPU analytics
///////////////////////////////////////////////////////////

static uint32_t s_last_ticks = 0;

void dump_current_runtime_stats(void) {
  uint32_t stop_ticks = s_analytics_stop_ticks;
  uint32_t sleep_ticks = s_analytics_sleep_ticks;

  uint32_t now_ticks = rtc_get_ticks();
  uint32_t total_ticks = now_ticks - s_last_ticks;
  uint32_t running_ticks = total_ticks - stop_ticks - sleep_ticks;

  char buf[160];
  snprintf(buf, sizeof(buf), "Run:   %"PRIu32" ticks (%"PRIu32" %%)",
           running_ticks, (running_ticks * 100) / total_ticks);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Sleep: %"PRIu32" ticks (%"PRIu32" %%)",
           sleep_ticks, (sleep_ticks * 100) / total_ticks);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Stop:  %"PRIu32" ticks (%"PRIu32" %%)",
           stop_ticks, (stop_ticks * 100) / total_ticks);
  prompt_send_response(buf);
  snprintf(buf, sizeof(buf), "Tot:   %"PRIu32" ticks", total_ticks);
  prompt_send_response(buf);

  uint32_t rtc_ticks = rtc_get_ticks();
  uint32_t rtos_ticks = xTaskGetTickCount();
  snprintf(buf, sizeof(buf), "RTC ticks: %"PRIu32", RTOS ticks: %"PRIu32 ", ticks corrected: %"PRIu32 ", last ticks stopped: %"PRIu32 " / %"PRIu32,
                             rtc_ticks, rtos_ticks, s_ticks_corrected, s_last_ticks_elapsed_in_stop, s_last_ticks_commanded_in_stop);
  prompt_send_response(buf);
}

void pbl_analytics_external_collect_cpu_stats(void) {
  uint32_t stop_ticks = s_analytics_stop_ticks;
  uint32_t sleep_ticks = s_analytics_sleep_ticks;

  RtcTicks now_ticks = rtc_get_ticks();
  uint32_t total_ticks = (uint32_t)(now_ticks - s_last_ticks);
  uint32_t running_ticks = total_ticks - stop_ticks - sleep_ticks;

  // Calculate percentages
  uint16_t running_pct = 0;
  uint16_t stop_pct = 0;
  uint16_t sleep_pct = 0;

  if (total_ticks > 0) {
    running_pct = (uint16_t)((running_ticks * 10000ULL) / total_ticks);
    stop_pct = (uint16_t)((stop_ticks * 10000ULL) / total_ticks);
    sleep_pct = (uint16_t)((sleep_ticks * 10000ULL) / total_ticks);
  }

  // NRF5: sleep0 = light sleep (WFI), sleep1 = stop mode, sleep2 = unused
  PBL_ANALYTICS_SET_UNSIGNED(cpu_running_pct, running_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep0_pct, sleep_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep1_pct, stop_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep2_pct, 0);

  s_last_ticks = now_ticks;
  s_analytics_sleep_ticks = 0;
  s_analytics_stop_ticks = 0;
}