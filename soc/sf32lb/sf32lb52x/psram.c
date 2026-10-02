/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "psram.h"

#include "pbl/mcu/cache.h"
#include "system/passert.h"

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

#include <bf0_hal.h>

#include <inttypes.h>
#include <stdint.h>

typedef struct {
  const char *name;
  uint8_t mode;
} PsramType;

// Indexed by the package ID in HPSYS_CFG->IDR
static const PsramType s_types[8] = {
  [2] = {"APS 128Mb OPI", SPI_MODE_OPSRAM},
  [3] = {"APS 64Mb OPI", SPI_MODE_OPSRAM},
  [4] = {"APS 32Mb legacy OPI", SPI_MODE_LEGPSRAM},
  [6] = {"Winbond HyperBus", SPI_MODE_HBPSRAM},
};

static FLASH_HandleTypeDef s_handle;
static const PsramType *s_type;

static uint8_t prv_get_pid(void) {
  return ((hwp_hpsys_cfg->IDR & HPSYS_CFG_IDR_PID_Msk) >> HPSYS_CFG_IDR_PID_Pos) & 0x7;
}

static void prv_pinmux(uint8_t pid) {
  if (pid == 2) {
    HAL_PIN_Set(PAD_SA01, MPI1_DIO0, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA02, MPI1_DIO1, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA03, MPI1_DIO2, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA04, MPI1_DIO3, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA05, MPI1_DIO4, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA06, MPI1_DIO5, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA07, MPI1_DIO6, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA08, MPI1_DIO7, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA09, MPI1_DQSDM, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA10, MPI1_CLK, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA11, MPI1_CS, PIN_NOPULL, 1);
    HAL_PIN_Set_Analog(PAD_SA00, 1);
    HAL_PIN_Set_Analog(PAD_SA12, 1);
    return;
  }

  HAL_PIN_Set(PAD_SA01, MPI1_DIO0, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA02, MPI1_DIO1, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA03, MPI1_DIO2, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA04, MPI1_DIO3, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA08, MPI1_DIO4, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA09, MPI1_DIO5, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA10, MPI1_DIO6, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA11, MPI1_DIO7, PIN_PULLDOWN, 1);
  HAL_PIN_Set(PAD_SA07, MPI1_CLK, PIN_NOPULL, 1);
  HAL_PIN_Set(PAD_SA05, MPI1_CS, PIN_NOPULL, 1);

  if (pid == 4) {
    HAL_PIN_Set(PAD_SA00, MPI1_DM, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA12, MPI1_DQS, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA06, MPI1_CLKB, PIN_NOPULL, 1);
  } else {
    HAL_PIN_Set(PAD_SA12, MPI1_DQSDM, (pid == 6) ? PIN_NOPULL : PIN_PULLDOWN, 1);
    HAL_PIN_Set_Analog(PAD_SA00, 1);
    HAL_PIN_Set_Analog(PAD_SA06, 1);
  }
}

void soc_sf32lb_psram_init(void) {
  HAL_StatusTypeDef ret;
  uint8_t pid;

  pid = prv_get_pid();
  s_type = &s_types[pid];
  PBL_ASSERT(s_type->name != NULL, "Unsupported SiP PSRAM (PID %u)", pid);

  prv_pinmux(pid);

  // MPI1 runs from SYSCLK (DLL1, 240 MHz): 120 MHz DDR on the bus
  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_PSRAM1, RCC_CLK_FLASH_SYSCLK);

  qspi_configure_t cfg = {
    .Instance = FLASH1,
    .base = MPI1_MEM_BASE,
    .msize = CONFIG_EXTRAM_SIZE / 0x100000,
    .SpiMode = s_type->mode,
  };

  ret = HAL_MPI_PSRAM_Init(&s_handle, &cfg, 1);
  PBL_ASSERTN(ret == HAL_OK);
}

void soc_sf32lb_psram_enter_low_power(void) {
#ifdef CONFIG_SF32LB52_PSRAM_HALF_SLEEP
  // No write-back may reach the PSRAM while it sleeps
  dcache_flush_all();
  HAL_MPI_PSRAM_ENT_LOWP(&s_handle, s_type->mode);
#endif
}

void soc_sf32lb_psram_exit_low_power(void) {
#ifdef CONFIG_SF32LB52_PSRAM_HALF_SLEEP
  HAL_MPI_EXIT_LOWP(&s_handle, s_type->mode);
#endif
}

#ifdef CONFIG_SHELL
extern uint8_t __extram_bss_start[];
extern uint8_t __extram_bss_end[];

static int prv_cmd_psram(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "Type: %s (PID %u)", s_type->name, prv_get_pid());
  pbl_shell_print(sh, "Used: %" PRIu32 " of %" PRIu32 " bytes",
                  (uint32_t)(__extram_bss_end - __extram_bss_start), (uint32_t)CONFIG_EXTRAM_SIZE);
#ifdef CONFIG_SF32LB52_PSRAM_HALF_SLEEP
  pbl_shell_print(sh, "Deep sleep: half-sleep");
#else
  pbl_shell_print(sh, "Deep sleep: standby");
#endif
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, psram, NULL, "Show PSRAM information", prv_cmd_psram, 0, 0);
#endif
