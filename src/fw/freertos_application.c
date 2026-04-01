/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "debug/power_tracking.h"
#include "drivers/mcu.h"
#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"

#include "console/prompt.h"
#include "kernel/memory_layout.h"
#include "kernel/pbl_malloc.h"
#include "os/tick.h"
#include "kernel/util/stop.h"
#include "kernel/util/wfi.h"
#include "process_management/worker_manager.h"
#include "services/common/analytics/analytics.h"
#include "system/logging.h"
#include "util/math.h"

#include <cmsis_core.h>

#if defined(MICRO_FAMILY_NRF5)
#include <hal/nrf_nvmc.h>
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "freertos_application.h"

static RtcTicks s_analytics_sleep_ticks = 0;
static RtcTicks s_analytics_stop_ticks = 0;

static uint32_t s_last_ticks_elapsed_in_stop = 0;
static uint32_t s_last_ticks_commanded_in_stop = 0;
static uint32_t s_ticks_corrected = 0;

// We need different timings for our different platforms since we use different mechanisms to keep
// time and to wake us up out of stop mode. On stm32f2 we don't have a millisecond register so we
// use the "retina rtc" and a RTC Alarm peripheral. On stm32f4 we do have a millisecond register
// so use the RTC running at normal speed and a RTC Wakeup peripheral. These have different
// accuracies when going into and out of stop mode.
#if defined(MICRO_FAMILY_STM32F4)
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks EARLY_WAKEUP_TICKS = 4;
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks MIN_STOP_TICKS = 8;
#elif defined(MICRO_FAMILY_NRF5)
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks EARLY_WAKEUP_TICKS = 2;
//! Stop mode until this number of ticks before the next scheduled task
static const RtcTicks MIN_STOP_TICKS = 5;
#endif

// 1 second ticks so that we only wake up once every regular timer interval.
static const RtcTicks MAX_STOP_TICKS = RTC_TICKS_HZ;

#if !defined(MICRO_FAMILY_SF32LB52)
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

#if defined(MICRO_FAMILY_NRF5)
  // We're going to sleep, so turn off the caches (they consume quiescent
  // power).  It's more efficient to have them on when we're awake, but for
  // now, they gotta go.  This holds true even if we're not going to sleep
  // long enough to trigger stop mode.
  NRF_NVMC->ICACHECNF &= ~NVMC_ICACHECNF_CACHEEN_Msk;
#endif

  power_tracking_stop(PowerSystemMcuCoreRun);

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    if (xExpectedIdleTime < MIN_STOP_TICKS || !stop_mode_is_allowed()) {
      RtcTicks sleep_start_ticks = rtc_get_ticks();

      power_tracking_start(PowerSystemMcuCoreSleep);
      __DSB();  // Drain any pending memory writes before entering sleep.
      do_wfi();  // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
      __ISB();  // Let the pipeline catch up (force the WFI to activate before moving on).
      power_tracking_stop(PowerSystemMcuCoreSleep);

      s_analytics_sleep_ticks += rtc_get_ticks() - sleep_start_ticks;
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

      s_analytics_stop_ticks += ticks_elapsed;
    }
  }

  power_tracking_start(PowerSystemMcuCoreRun);

#if defined(MICRO_FAMILY_NRF5)
  NRF_NVMC->ICACHECNF |= NVMC_ICACHECNF_CACHEEN_Msk;
#endif

  __enable_irq();
}
#endif

void vApplicationStackOverflowHook(TaskHandle_t task_handle, signed char *name) {
  PebbleTask task = pebble_task_get_task_for_handle(task_handle);

  // If the task is application or worker, ignore this hook. We have a memory protection region
  // setup at the bottom of those stacks and the code that catches MPU violiations to that
  // area in fault_handling.c has the logic to safely kill those user tasks without forcing
  // a reboot.
  if ((task != PebbleTask_App) && (task != PebbleTask_Worker)) {
    PBL_LOG_SYNC_ERR("Stack overflow [task: %s]", name);
    RebootReason reason = {
      .code = RebootReasonCode_StackOverflow,
      .data8[0] = task
    };
    reboot_reason_set(&reason);

    reset_due_to_software_failure();
  }
}

bool xApplicationIsAllowedToRaisePrivilege(uint32_t caller_pc) {
  // This function is called by portSVCHandler with the PC value of the
  // function which initiated the SVC call requesting privilege elevation.

  // The memory_region.c functions are not used for this check as this function
  // is in a hot code-path and needs to execute as quickly as possible.

  // All syscall functions are lumped together in one place in the firmware
  // image to reduce the attack surface. Don't allow privilege to be raised by
  // any code outside of that region, even if that code is in flash.
  // See WHT-114 and PBL-34044.
  extern const uint32_t __syscall_text_start__[];
  extern const uint32_t __syscall_text_end__[];
  const uint32_t priv_code_start = (uint32_t) __syscall_text_start__;
  const uint32_t priv_code_end = (uint32_t) __syscall_text_end__;
  return (caller_pc >= priv_code_start && caller_pc < priv_code_end);
}

#undef vPortFree
void vPortFree(void* pv) {
  kernel_free(pv);
}

#undef pvPortMalloc
void* pvPortMalloc(size_t xSize) {
  return kernel_malloc(xSize);
}

#if !defined(MICRO_FAMILY_SF32LB52)
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
#endif

#if !defined(MICRO_FAMILY_SF32LB52)
bool vPortEnableTimer() {
#if defined(MICRO_FAMILY_NRF5)
  rtc_enable_synthetic_systick();
  return true;
#else
  return false;
#endif
}
#endif

// CPU analytics
///////////////////////////////////////////////////////////

static uint32_t s_last_ticks = 0;

#if !defined(MICRO_FAMILY_SF32LB52)
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

void analytics_external_collect_cpu_stats(void) {
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

  // STM32/NRF5: sleep0 = light sleep (WFI), sleep1 = stop mode, sleep2 = unused
  PBL_ANALYTICS_SET_UNSIGNED(cpu_running_pct, running_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep0_pct, sleep_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep1_pct, stop_pct);
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep2_pct, 0);

  s_last_ticks = now_ticks;
  s_analytics_sleep_ticks = 0;
  s_analytics_stop_ticks = 0;
}
#endif