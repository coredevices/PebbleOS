/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "psram.h"

#include <pbl/kernel/assert.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/psram.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/analytics/analytics.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

#include <inttypes.h>
#include <stdint.h>

#include <bf0_hal.h>

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

typedef enum {
  PsramStateActive,
  PsramStateHalfSleep,
  PsramStateDeepPowerDown,
} PsramState;

static FLASH_HandleTypeDef s_handle;
static qspi_configure_t s_cfg;
static const PsramType *s_type;

// Holds are counted under s_lock. The idle thread reads them, and moves the PSRAM between states,
// with interrupts masked and every other thread blocked, so it never sees a transition halfway.
static PBL_MUTEX_DEFINE(s_lock);
static uint32_t s_holds;
static volatile PsramState s_state;
static volatile uint32_t s_epoch;
static uint32_t s_dpd_wakes;

extern uint8_t __extram_bss_start[];
extern uint8_t __extram_bss_end[];
extern uint8_t __disposable_bss_start[];
extern uint8_t __disposable_bss_end[];

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

  s_cfg = (qspi_configure_t){
    .Instance = FLASH1,
    .base = MPI1_MEM_BASE,
    .msize = CONFIG_EXTRAM_SIZE / 0x100000,
    .SpiMode = s_type->mode,
  };

  ret = HAL_MPI_PSRAM_Init(&s_handle, &s_cfg, 1);
  PBL_ASSERTN(ret == HAL_OK);
}

static bool prv_dpd_supported(void) {
#ifdef CONFIG_SF32LB52_PSRAM_HALF_SLEEP
  return false;
#else
  // Only disposable data may be lost
  if (__extram_bss_end - __extram_bss_start != 0) {
    return false;
  }
  // Legacy OPI parts have no deep power-down path
  return s_type->mode != SPI_MODE_LEGPSRAM;
#endif
}

static void prv_wake_from_dpd(void) {
  HAL_StatusTypeDef ret;

  ret = HAL_MPI_EXIT_LOWP(&s_handle, s_type->mode);
  PBL_ASSERTN(ret == HAL_OK);

  // Deep power-down resets the PSRAM configuration registers
  ret = HAL_MPI_PSRAM_Init(&s_handle, &s_cfg, 1);
  PBL_ASSERTN(ret == HAL_OK);

  s_state = PsramStateActive;
  s_dpd_wakes++;
  PBL_ANALYTICS_ADD(psram_epoch_resets, 1);
}

void psram_retain_acquire(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_holds++ == 0U) {
    PBL_ANALYTICS_TIMER_START(psram_hold_ms);
  }
  if (s_state == PsramStateDeepPowerDown) {
    prv_wake_from_dpd();
  }
  pbl_mutex_unlock(&s_lock);
}

void psram_retain_release(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(s_holds > 0U);
  if (--s_holds == 0U) {
    PBL_ANALYTICS_TIMER_STOP(psram_hold_ms);
  }
  pbl_mutex_unlock(&s_lock);
}

bool psram_is_retained(void) {
  return s_holds > 0U;
}

uint32_t psram_epoch(void) {
  return s_epoch;
}

void soc_sf32lb_psram_enter_low_power(void) {
  HAL_StatusTypeDef ret;

  if (s_state != PsramStateActive) {
    return;
  }

  if ((s_holds > 0U) || !prv_dpd_supported()) {
    // No write-back may reach the PSRAM while it sleeps
    dcache_flush_all();
    ret = HAL_MPI_PSRAM_ENT_LOWP(&s_handle, s_type->mode);
    PBL_ASSERTN(ret == HAL_OK);
    s_state = PsramStateHalfSleep;
    return;
  }

  // Nor may a dirty line be evicted into it, or a stale one be hit after it is powered back up
  dcache_flush_invalidate_all();
  if (s_type->mode == SPI_MODE_HBPSRAM) {
    ret = HAL_HYPER_PSRAM_DPD(&s_handle);
  } else {
    ret = HAL_MPI_PSRAM_DPD(&s_handle);
  }
  PBL_ASSERTN(ret == HAL_OK);
  // Without a pause before the clocks are switched off, the HCPU sometimes hangs until the
  // watchdog resets it
  HAL_Delay_us(100);
  s_state = PsramStateDeepPowerDown;
  s_epoch++;
}

void soc_sf32lb_psram_exit_low_power(void) {
  HAL_StatusTypeDef ret;

  // Left in deep power-down until the next hold
  if (s_state == PsramStateHalfSleep) {
    ret = HAL_MPI_EXIT_LOWP(&s_handle, s_type->mode);
    PBL_ASSERTN(ret == HAL_OK);
    s_state = PsramStateActive;
  }
}

#ifdef CONFIG_SHELL
static int prv_cmd_psram(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "Type: %s (PID %u)", s_type->name, prv_get_pid());
  pbl_shell_print(sh, "Used: %" PRIu32 " retained + %" PRIu32 " disposable of %" PRIu32 " bytes",
                  (uint32_t)(__extram_bss_end - __extram_bss_start),
                  (uint32_t)(__disposable_bss_end - __disposable_bss_start),
                  (uint32_t)CONFIG_EXTRAM_SIZE);
  pbl_shell_print(sh, "Holds: %" PRIu32 ", epoch: %" PRIu32 ", deep power-down wakes: %" PRIu32,
                  s_holds, s_epoch, s_dpd_wakes);
  pbl_shell_print(sh, "State: %s",
                  (s_state == PsramStateDeepPowerDown) ? "deep power-down" : "active");
  pbl_shell_print(sh, "Deep sleep without holds: %s",
                  prv_dpd_supported() ? "deep power-down" : "half-sleep");
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, psram, NULL, "Show PSRAM information", prv_cmd_psram, 0, 0);
#endif
