/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/watchdog.h"
#include "system/passert.h"

#include <bf0_hal.h>

#define HCPU_FREQ_MHZ 240
#define PWRKEY_RESET_CNT (32000 * 15)

#define PBL_PSRAM_SIZE_MB 16
#define PSRAM_MPI1_DIV_OPI 1
#define PSRAM_MPI1_DIV_QSPI 2

static void prv_psram_init(void) {
  HAL_RCC_HCPU_EnableDLL2(288000000);
  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_FLASH1, RCC_CLK_FLASH_DLL2);

  // PSRAM is powered by VDD_SiP; also enable the 1V8 peripheral LDO
  HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO_1V8, true, true);

  qspi_configure_t cfg = {
      .Instance = hwp_qspi1,
      .base = PSRAM_BASE,
      .msize = PBL_PSRAM_SIZE_MB,
  };

  uint16_t div = PSRAM_MPI1_DIV_QSPI;
  uint32_t pid = (hwp_hpsys_cfg->IDR & HPSYS_CFG_IDR_PID_Msk) >> HPSYS_CFG_IDR_PID_Pos;
  switch (pid & 7) {
    case 5:  // BOOT_PSRAM_APS_16P: 16Mb APM QSPI PSRAM
      cfg.SpiMode = SPI_MODE_PSRAM;
      break;
    case 4:  // BOOT_PSRAM_APS_32P: 32Mb APM legacy PSRAM
      cfg.SpiMode = SPI_MODE_LEGPSRAM;
      break;
    case 6:  // BOOT_PSRAM_WINBOND: Winbond HyperBus PSRAM
      cfg.SpiMode = SPI_MODE_HBPSRAM;
      break;
    case 2:  // BOOT_PSRAM_APS_128P: 128Mb (16MB) APM XCELLA OPI PSRAM
    case 3:  // BOOT_PSRAM_APS_64P:  64Mb APM XCELLA OPI PSRAM
      cfg.SpiMode = SPI_MODE_OPSRAM;
      div = PSRAM_MPI1_DIV_OPI;
      break;
    default:
      PBL_CROAK("Unexpected PSRAM PID %lu", pid);
  }

  static FLASH_HandleTypeDef f_handle;
  f_handle.wakeup = (SystemPowerOnModeGet() == PM_STANDBY_BOOT) ? 1 : 0;
  HAL_StatusTypeDef ret = HAL_MPI_PSRAM_Init(&f_handle, &cfg, div);
  PBL_ASSERTN(ret == HAL_OK);

  // Self-test
  volatile uint32_t *first = (volatile uint32_t *)PSRAM_BASE;
  volatile uint32_t *last =
      (volatile uint32_t *)(PSRAM_BASE + PBL_PSRAM_SIZE_MB * 1024 * 1024 - sizeof(uint32_t));
  *first = 0xA5A5A5A5U;
  *last = 0x5A5A5A5AU;
  __DSB();
  PBL_ASSERTN(*first == 0xA5A5A5A5U && *last == 0x5A5A5A5AU);
}

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

  // Disable default PA21 pull-down, causing leakage
  HAL_PIN_Set(PAD_PA21, GPIO_A21, PIN_NOPULL, 1);

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

#ifndef CONFIG_NO_WATCHDOG
  watchdog_init();
  watchdog_start();
#endif

  HAL_PMU_EnableDLL(1);
#ifdef SF32LB52_USE_LXT
  HAL_PMU_EnableXTAL32();
  ret = HAL_PMU_LXTReady();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RTC_ENABLE_LXT();
#endif

  HAL_RCC_LCPU_ClockSelect(RCC_CLK_MOD_LP_PERI, RCC_CLK_PERI_HXT48);

  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();

  ret = HAL_RCC_CalibrateRC48();
  PBL_ASSERTN(ret == HAL_OK);

  HAL_RCC_HCPU_ConfigHCLK(HCPU_FREQ_MHZ);

  // Reset sysclk used by HAL_Delay_us
  HAL_Delay_us(0);

  HAL_RCC_Init();
  HAL_PMU_Init();

  __HAL_SYSCFG_CLEAR_SECURITY();
  HAL_EFUSE_Init();

  //set Sifli chipset pwrkey reset time to 15s, so it always use PMIC cold reboot for long press 
  hwp_pmuc->PWRKEY_CNT = PWRKEY_RESET_CNT;

  // Bring up the MPI-PSRAM (pins SA00..SA12). APP_RAM/WORKER_RAM live there.
  prv_psram_init();

  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_REQ, AON_PIN_MODE_HIGH);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LP2HP_IRQ, AON_PIN_MODE_HIGH);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_LPTIM1, AON_PIN_MODE_HIGH);
  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_GPIO1, AON_PIN_MODE_HIGH);
}
