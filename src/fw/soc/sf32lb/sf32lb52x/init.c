/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"
#include "system/passert.h"

#include <bf0_hal.h>

#define HCPU_FREQ_MHZ 240
#define PWRKEY_RESET_CNT (32000 * 15)

void soc_early_init(void) {
  HAL_StatusTypeDef ret;
  uint32_t bootopt;

  // Adjust bootrom pull-up/down delays on PA21 (flash power control pin) so
  // that the flash is properly power cycled on reset. A flash power cycle is
  // needed if left in 4-byte addressing mode, as bootrom does not support it.
  bootopt = HAL_Get_backup(RTC_BACKUP_BOOTOPT);
  bootopt &= ~(RTC_BACKUP_BOOTOPT_PD_DELAY_Msk | RTC_BACKUP_BOOTOPT_PU_DELAY_Msk);
  bootopt |= RTC_BACKUP_BOOTOPT_PD_DELAY_MS(100) | RTC_BACKUP_BOOTOPT_PU_DELAY_MS(10);
  HAL_Set_backup(RTC_BACKUP_BOOTOPT, bootopt);

  if (HAL_RCC_HCPU_GetClockSrc(RCC_CLK_MOD_SYS) == RCC_SYSCLK_HRC48) {
    HAL_HPAON_EnableXT48();
    HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HXT48);
  }

  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_PERI, RCC_CLK_PERI_HXT48);

  // Halt LCPU first to avoid LCPU in running state
  HAL_HPAON_WakeCore(CORE_ID_LCPU);
  HAL_RCC_Reset_and_Halt_LCPU(1);

  // Load system configuration from EFUSE
  BSP_System_Config();

  HAL_HPAON_StartGTimer();

  HAL_PMU_EnableRC32K(1);

  // Stop and restart WDT in case it was clocked by RC10K before
  watchdog_stop();

  HAL_PMU_LpCLockSelect(PMU_LPCLK_RC32);

  watchdog_init();
  watchdog_start();

  HAL_PMU_EnableDLL(1);
#ifdef SF32LB52_USE_LXT
  HAL_PMU_EnableXTAL32();
  ret = HAL_PMU_LXTReady();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RTC_ENABLE_LXT();
#endif

  HAL_RCC_LCPU_ClockSelect(RCC_CLK_MOD_LP_PERI, RCC_CLK_PERI_HXT48);

  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();

  HAL_RCC_HCPU_ConfigHCLK(HCPU_FREQ_MHZ);

  // Reset sysclk used by HAL_Delay_us
  HAL_Delay_us(0);

  ret = HAL_RCC_CalibrateRC48();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RCC_Init();
  HAL_PMU_Init();

  __HAL_SYSCFG_CLEAR_SECURITY();
  HAL_EFUSE_Init();

  //set Sifli chipset pwrkey reset time to 15s, so it always use PMIC cold reboot for long press 
  hwp_pmuc->PWRKEY_CNT = PWRKEY_RESET_CNT;

  // Disable 1V8 LDO (feeds PSRAM, we use VDD_SiP to power it)
  hwp_pmuc->PERI_LDO &= ~(PMUC_PERI_LDO_EN_LDO18_Msk | PMUC_PERI_LDO_LDO18_PD_Msk);
  hwp_pmuc->PERI_LDO |= PMUC_PERI_LDO_LDO18_PD_Msk;

  // Set all PSRAM pins as analog (low-power)
  HAL_PIN_Set_Analog(PAD_SA00, 1);
  HAL_PIN_Set_Analog(PAD_SA01, 1);
  HAL_PIN_Set_Analog(PAD_SA02, 1);
  HAL_PIN_Set_Analog(PAD_SA03, 1);
  HAL_PIN_Set_Analog(PAD_SA04, 1);
  HAL_PIN_Set_Analog(PAD_SA05, 1);
  HAL_PIN_Set_Analog(PAD_SA06, 1);
  HAL_PIN_Set_Analog(PAD_SA07, 1);
  HAL_PIN_Set_Analog(PAD_SA08, 1);
  HAL_PIN_Set_Analog(PAD_SA09, 1);
  HAL_PIN_Set_Analog(PAD_SA10, 1);
  HAL_PIN_Set_Analog(PAD_SA11, 1);
  HAL_PIN_Set_Analog(PAD_SA12, 1);
}
