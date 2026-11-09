/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/section.h>
#include <pbl/kernel/time.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

#include <bf0_hal.h>

enum {
  IDLE_WFI,
  IDLE_DEEPWFI,
  IDLE_DEEPSLEEP,
};

static bool s_force_wfi;

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

PBL_SECTION_RAM static void prv_enter_wfi(void) {
  __DSB();
  __WFI();
  __ISB();
}

PBL_SECTION_RAM static void prv_enter_deepwfi(void) {
  __DSB();
  __ISB();

  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __WFI();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

// Runs with the flash powered down
PBL_SECTION_RAM static void prv_deepsleep(void *arg) {
  uint32_t dll1_freq = 0UL;
  int clk_src;

  NVIC_EnableIRQ(AON_IRQn);

  clk_src = HAL_RCC_HCPU_GetClockSrc(RCC_CLK_MOD_SYS);
  if (clk_src == RCC_SYSCLK_DLL1) {
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HRC48);
    dll1_freq = HAL_RCC_HCPU_GetDLL1Freq();
    HAL_RCC_HCPU_DisableDLL1();
  }

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
  if (dll1_freq != 0UL) {
    while (0 == (hwp_hpsys_aon->ACR & HPSYS_AON_ACR_HXT48_RDY)) {
      __NOP();
    }
  }

  if (clk_src == RCC_SYSCLK_DLL1) {
    HAL_RCC_HCPU_EnableDLL1(dll1_freq);
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_DLL1);
  }
}

PBL_SECTION_RAM static void prv_enter_deepsleep(void) {
  prv_save_iser();
  pbl_flash_power_down(FLASH, prv_deepsleep, nullptr);
  prv_restore_iser();
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter_wfi},
  [IDLE_DEEPWFI] = {.name = "deepwfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter_deepwfi},
  [IDLE_DEEPSLEEP] = {
    .name = "deepsleep",
    .min_ticks = PBL_TICK_HZ / 20,
    .exit_ticks = 4,
    .enter = prv_enter_deepsleep
  },
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

static_assert(IDLE_WFI == SOC_SF32LB_WFI - 1 && IDLE_DEEPSLEEP == SOC_SF32LB_DEEPSLEEP - 1,
              "idle states follow the sleep levels");

PBL_SECTION_RAM int pbl_soc_idle_state_limit(void) {
  // The debug flag forces plain WFI (deep WFI and deep sleep disabled).
  if (s_force_wfi) {
    return IDLE_WFI;
  }

  return (int)soc_sf32lb_sleep_max_level() - 1;
}

PBL_IRQ_DIRECT(AON, 0, PBL_IRQ_ZERO_LATENCY) {
  uint32_t status;

  NVIC_DisableIRQ(AON_IRQn);
  HAL_HPAON_CLEAR_POWER_MODE();

  status = HAL_HPAON_GET_WSR();
  status &= ~HPSYS_AON_WSR_PIN_ALL;
  HAL_HPAON_CLEAR_WSR(status);
}

#ifdef CONFIG_SHELL
static int prv_cmd_wfi(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argv[1][0] == '1') {
    s_force_wfi = true;
    pbl_shell_print(sh, "WFI forced ON (deep WFI and deep sleep disabled)");
  } else {
    s_force_wfi = false;
    pbl_shell_print(sh, "WFI forced OFF (deep WFI and deep sleep allowed)");
  }
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, wfi, nullptr, "Force plain WFI when idle <0|1>", prv_cmd_wfi, 2, 0);
#endif
