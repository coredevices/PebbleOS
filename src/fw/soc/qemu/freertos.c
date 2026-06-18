/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"
#include "kernel/util/stop.h"
#include "kernel/util/wfi.h"
#include "util/math.h"

#include <cmsis_core.h>

#include "FreeRTOS.h"
#include "task.h"

static const RtcTicks EARLY_WAKEUP_TICKS = 2;
static const RtcTicks MIN_STOP_TICKS = 5;

// 1 second ticks so that we only wake up once every regular timer interval.
static const RtcTicks MAX_STOP_TICKS = RTC_TICKS_HZ;

static uint32_t s_last_ticks_elapsed_in_stop = 0;
static uint32_t s_last_ticks_commanded_in_stop = 0;
static uint32_t s_ticks_corrected = 0;

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

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    if (xExpectedIdleTime < MIN_STOP_TICKS || !stop_mode_is_allowed()) {
      __DSB();  // Drain any pending memory writes before entering sleep.
      do_wfi();  // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
      __ISB();  // Let the pipeline catch up (force the WFI to activate before moving on).
    } else {
      const RtcTicks stop_duration = MIN(xExpectedIdleTime - EARLY_WAKEUP_TICKS, MAX_STOP_TICKS);

      // Go into stop mode until the wakeup_tick.
      s_last_ticks_commanded_in_stop = stop_duration;

      rtc_alarm_set(stop_duration);
      enter_stop_mode();

      RtcTicks ticks_elapsed = rtc_alarm_get_elapsed_ticks();

      s_last_ticks_elapsed_in_stop = ticks_elapsed;
      vTaskStepTick(ticks_elapsed);

      // Update the task watchdog every time we come out of STOP mode (which is
      // at least once/second) since the timer peripheral will not have been
      // incremented
      task_watchdog_step_elapsed_time_ms((ticks_elapsed * 1000) / RTC_TICKS_HZ);
    }
  }

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
  return false;
}

void dump_current_runtime_stats(void) {
}

void pbl_analytics_external_collect_cpu_stats(void) {
}