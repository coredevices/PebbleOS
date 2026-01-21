/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdio.h>

#include "console/prompt.h"
#include "drivers/flash.h"
#include "drivers/mcu.h"
#include "drivers/rtc.h"
#include "drivers/sf32lb52/rc10k.h"
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

static LPTIM_HandleTypeDef s_lptim = {
    .Instance = LPTIM1,
};

//! Early wake-up ticks (to avoid over-sleeping due to wake-up latency)
static const uint32_t EARLY_WAKEUP_TICKS = 4;
//! Minimum ticks to enter deep sleep
static const uint32_t MIN_DEEPSLEEP_TICKS = RTC_TICKS_HZ / 10;
// Maximum ticks allowed for deep sleep (1 second, for regular timer)
static const uint32_t MAX_DEEPSLEEP_TICKS = RTC_TICKS_HZ;

static uint32_t s_iser_bak[16];

static void prv_save_iser(void) {
  uint32_t i;
  for (i = 0; i < 16; i++) {
    s_iser_bak[i] = NVIC->ISER[i];
    NVIC->ICER[i] = 0xFFFFFFFF;
    __DSB();
    __ISB();
  }
}

static void prv_restore_iser(void) {
  uint32_t i;
  for (i = 0; i < 16; i++) {
    __COMPILER_BARRIER();
    NVIC->ISER[i] = s_iser_bak[i];
    __COMPILER_BARRIER();
  }
}

static inline void prv_enter_wfi(void) {
  __WFI();
}

static void prv_enter_deepwfi(void) {
  flash_power_down_for_stop_mode();

  __DSB();
  __ISB();

  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __WFI();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

static void prv_enter_deepslep(void) {
  uint32_t dll1_freq;
  uint32_t dll2_freq;
  int clk_src;

  prv_save_iser();

  flash_power_down_for_stop_mode();

  NVIC_EnableIRQ(AON_IRQn);

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

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
  if (!sleep_mode_is_allowed() || !ipc_queue_check_idle()) {
    return;
  }

  __disable_irq();

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    if (!stop_mode_is_allowed()) {
      prv_enter_wfi();
    } else {
      if (xExpectedIdleTime < MIN_DEEPSLEEP_TICKS) {
        prv_enter_deepwfi();
      } else {
        uint32_t gtimer_start;
        uint32_t gtimer_stop;
        uint32_t gtimer_delta;
        uint32_t sleep_ticks;
        uint32_t lptim_ticks;
        uint32_t elapsed_ticks;

        // stop systick
        SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
        SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

        // configure LPTIM to wake us up after expected idle time
        sleep_ticks = MIN(xExpectedIdleTime - EARLY_WAKEUP_TICKS, MAX_DEEPSLEEP_TICKS);
        lptim_ticks = sleep_ticks * rc10k_get_freq_hz() / RTC_TICKS_HZ;
        HAL_LPTIM_Counter_Start_IT(&s_lptim, lptim_ticks);

        gtimer_start = HAL_GTIMER_READ();

        prv_enter_deepslep();

        // NOTE: GTIMER needs at least 1 LP clock cycle to be updated after sleep,
        // so spin until we see a change
        do {
          gtimer_stop = HAL_GTIMER_READ();
        } while (gtimer_stop == gtimer_start);

        if (gtimer_stop < gtimer_start) {
          gtimer_delta = (UINT32_MAX - gtimer_start) + gtimer_stop + 1UL;
        } else {
          gtimer_delta = gtimer_stop - gtimer_start;
        }

        elapsed_ticks = (gtimer_delta * RTC_TICKS_HZ) / rc10k_get_freq_hz();
        vTaskStepTick(elapsed_ticks);

        // Update the task watchdog every time we come out of STOP mode (which is
        // at least once/second) since the timer peripheral will not have been
        // incremented. Set all watchdog bits first since the LPTIM ISR that would
        // normally do this hasn't run yet (interrupts are still globally disabled).
        task_watchdog_bit_set_all();
        task_watchdog_step_elapsed_time_ms((elapsed_ticks * 1000) / RTC_TICKS_HZ);

        // stop LPTIM
        HAL_LPTIM_Counter_Stop_IT(&s_lptim);

        // enable systick
        SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
      }
    }
  }

  __enable_irq();
}

bool vPortEnableTimer() {
  HAL_LPTIM_InitDefault(&s_lptim);
  HAL_LPTIM_Init(&s_lptim);

  // configure SYSTICK
  // - use HXT48 as TICK reference clock
  // - divide TICK reference clock to 1.92MHz (48MHz / 25) (TICK_CLK)
  // - enable TICK_CLK as SYSTICK clock source
  // - configure SYSTICK to generate interrupt at RTC_TICKS_HZ rate
  //
  // HXT48│\                        ┌────────────────┐
  //  ────┼ \                       │         SYSTICK│
  //      │  │  ┌───────┐  TICK_CLK ││\              │
  //   ...│  ├──│TICKDIV├───────────┼┤ │   ┌──────┐  │
  //      │  │  └───────┘.         ┌┼┤ ├───│RELOAD├──┤
  //      │ /                      │││/    └──────┘  │
  //      │/                       ││                │
  //           HCLK                ││                │
  //          ─────────────────────┘└────────────────┘

  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_TICK, RCC_CLK_TICK_HXT48);
  // delay to avoid systick config failure (undocumented silicon issue)
  HAL_Delay_us(200);
  MODIFY_REG(hwp_hpsys_rcc->CFGR, HPSYS_RCC_CFGR_TICKDIV_Msk,
             MAKE_REG_VAL(25, HPSYS_RCC_CFGR_TICKDIV_Msk, HPSYS_RCC_CFGR_TICKDIV_Pos));
  HAL_SYSTICK_Config(1920000 / RTC_TICKS_HZ);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK_DIV8);

  SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);

  // configure clock dividers for deep WFI:
  // HCLK = 48MHz / 12 = 4MHz
  // PCLK1 = 4MHz / 2^0 = 4MHz
  // PCLK2 = 4MHz / 2^1 = 2MHz
  HAL_RCC_HCPU_SetDeepWFIDiv(12, 0, 1);

  // TODO(SF32LB52): to use deep WFI when audio is ON, HCLK needs to remain
  // at 48MHz (div=1). Also, clock needs to be forced ON during deep WFI
  // (FORCE_HP bit in HPSYS_RCC_DBGR register)

  return true;
}

void AON_IRQHandler(void)
{
    uint32_t status;

    NVIC_DisableIRQ(AON_IRQn);
    HAL_HPAON_CLEAR_POWER_MODE();

    status = HAL_HPAON_GET_WSR();
    status &= ~HPSYS_AON_WSR_PIN_ALL;
    HAL_HPAON_CLEAR_WSR(status);
}

void dump_current_runtime_stats(void) {}
