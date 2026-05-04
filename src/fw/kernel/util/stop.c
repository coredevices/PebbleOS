/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/dbgserial.h"
#include "console/dbgserial_input.h"
#include "drivers/flash.h"
#include "drivers/periph_config.h"
#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"
#include "os/tick.h"
#include "kernel/util/stop.h"
#include "kernel/util/wfi.h"
#include "mcu/interrupts.h"
#include "pbl/services/analytics/analytics.h"
#include "system/passert.h"
#include "console/dbgserial_input.h"

#include <cmsis_core.h>

#include <stdbool.h>
#include <inttypes.h>

static int s_num_items_disallowing_stop_mode = 0;

#ifdef PBL_NOSLEEP
static bool s_sleep_mode_allowed = false;
#else
static bool s_sleep_mode_allowed = true;
#endif

typedef struct {
  uint32_t active_count;
  RtcTicks ticks_when_stop_mode_disabled;
  RtcTicks total_ticks_while_disabled;
} InhibitorTickProfile;

// Note: These variables should be protected within a critical section since
// they are read and modified by multiple threads
static InhibitorTickProfile s_inhibitor_profile[InhibitorNumItems];

#if MICRO_FAMILY_NRF52
void enter_stop_mode(void) {
  dbgserial_enable_rx_exti();
  dbgserial_disable_rx_dma_before_stop();

  flash_power_down_for_stop_mode();
  rtc_systick_pause();

  /* XXX(nrf5): LATER: have MPSL turn off HFCLK */

  __DSB(); // Drain any pending memory writes before entering sleep.
  do_wfi(); // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
  __ISB(); // Let the pipeline catch up (force the WFI to activate before moving on).

  rtc_systick_resume();
  flash_power_up_after_stop_mode();

  dbgserial_enable_rx_dma_after_stop();
}
#elif defined(MICRO_FAMILY_QEMU)
void enter_stop_mode(void) {
}
#endif

void stop_mode_disable( StopModeInhibitor inhibitor ) {
  portENTER_CRITICAL();
  ++s_num_items_disallowing_stop_mode;

  ++s_inhibitor_profile[inhibitor].active_count;
  // TODO: We should probably check if s_inhibitor_profile.active_count == 1
  // before doing this assignment. We don't seem to ever run into this case
  // yet (i.e. active_count is never > 1), but when we do, this code would
  // report the wrong number of nostop ticks.
  s_inhibitor_profile[inhibitor].ticks_when_stop_mode_disabled = rtc_get_ticks();
  portEXIT_CRITICAL();
}

void stop_mode_enable( StopModeInhibitor inhibitor ) {
  portENTER_CRITICAL();
  PBL_ASSERTN(s_num_items_disallowing_stop_mode != 0);
  PBL_ASSERTN(s_inhibitor_profile[inhibitor].active_count != 0);

  --s_num_items_disallowing_stop_mode;
  --s_inhibitor_profile[inhibitor].active_count;
  if (s_inhibitor_profile[inhibitor].active_count == 0) {
    s_inhibitor_profile[inhibitor].total_ticks_while_disabled += rtc_get_ticks() -
        s_inhibitor_profile[inhibitor].ticks_when_stop_mode_disabled;
  }
  portEXIT_CRITICAL();
}

bool stop_mode_is_allowed(void) {
#if PBL_NOSTOP
  return false;
#else
  return s_num_items_disallowing_stop_mode == 0;
#endif
}

void sleep_mode_enable(bool enable) {
  s_sleep_mode_allowed = enable;
}

bool sleep_mode_is_allowed(void) {
#ifdef PBL_NOSLEEP
  return false;
#endif
  return s_sleep_mode_allowed;
}


void command_scheduler_force_active(void) {
  sleep_mode_enable(false);
}

void command_scheduler_resume_normal(void) {
  sleep_mode_enable(true);
}
