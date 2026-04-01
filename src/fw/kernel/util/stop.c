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
#include "services/common/analytics/analytics.h"
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

#if MICRO_FAMILY_NRF5
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
#elif MICRO_FAMILY_STM32F4
void enter_stop_mode(void) {
  // enable the interrupt on the debug RX line so that we can use the serial
  // console even when we are in stop mode.
  dbgserial_enable_rx_exti();

  flash_power_down_for_stop_mode();

  // Turn on the power control peripheral so that we can put the regulator into low-power mode
  periph_config_enable(PWR, RCC_APB1Periph_PWR);

  if (mcu_state_are_interrupts_enabled()) {
    // If INTs aren't disabled here, we would wind up servicing INTs
    // immediately after the WFI (while running at the wrong clock speed) which
    // can confuse peripherals in subtle ways
    WTF;
  }

  // Enter stop mode.
  //PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
  // We don't use ^^ the above function because of a silicon bug which
  // causes the processor to skip some instructions upon wake from STOP
  // in certain sitations. See the STM32F20x and STM32F21x Errata sheet
  // section 2.1.3 "Debugging Stop mode with WFE entry", or the erratum
  // of the same name in section 2.1.2 of the STM32F42x and STM32F43x
  // Errata sheet, for (misleading) details.
  // http://www.st.com/web/en/resource/technical/document/errata_sheet/DM00027213.pdf
  // http://www.st.com/web/en/resource/technical/document/errata_sheet/DM00068628.pdf

  // Configure the PWR peripheral to put us in low-power STOP mode when
  // the processor enters deepsleep.
  uint32_t temp = PWR->CR;
  temp &= ~PWR_CR_PDDS;
  temp |= PWR_CR_LPDS;
#if STM32F412xG
  // STM32F412xG suports a new "low-power regulator low voltage in deep sleep" mode.
  temp |= PWR_CR_LPLVDS;
#endif
  PWR->CR = temp;

  // Configure the processor core to enter deepsleep mode when we
  // execute a WFI or WFE instruction.
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  // Go stop now.
  __DSB(); // Drain any pending memory writes before entering sleep.
  do_wfi(); // Wait for Interrupt (enter sleep mode). Work around F2/F4 errata.
  __ISB(); // Let the pipeline catch up (force the WFI to activate before moving on).

  // Tell the processor not to emter deepsleep mode for future WFIs.
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

  // Stop mode will change our system clock to the HSI. Move it back to the PLL.

  // Enable the PLL and wait until it's ready
  RCC_PLLCmd(ENABLE);
  while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {}

  // Select PLL as system clock source and wait until it's being used
  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
  while (RCC_GetSYSCLKSource() != 0x08) {}

  // No longer need the power control peripheral
  periph_config_disable(PWR, RCC_APB1Periph_PWR);

  flash_power_up_after_stop_mode();
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

static RtcTicks prv_get_nostop_ticks(StopModeInhibitor inhibitor, RtcTicks now_ticks) {
    RtcTicks total_ticks = s_inhibitor_profile[inhibitor].total_ticks_while_disabled;
    if (s_inhibitor_profile[inhibitor].active_count != 0) {
        total_ticks += (now_ticks - s_inhibitor_profile[inhibitor].ticks_when_stop_mode_disabled);
    }
    return total_ticks;
}

void command_scheduler_force_active(void) {
  sleep_mode_enable(false);
}

void command_scheduler_resume_normal(void) {
  sleep_mode_enable(true);
}
