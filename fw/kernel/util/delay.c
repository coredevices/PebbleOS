/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "delay.h"

#include <pbl/kernel/compiler.h>

#ifdef CONFIG_SOC_NRF52
#include <drivers/nrfx_common.h>
#include <soc/nrfx_coredep.h>
#elifdef CONFIG_SOC_SF32LB52
#include <bf0_hal.h>
#elifdef CONFIG_SOC_POSIX
#include <unistd.h>
#endif

#ifdef CONFIG_SOC_NRF52
void PBL_NOINLINE delay_us(uint32_t us) {
  nrfx_coredep_delay_us(us);
}

void delay_init(void) {
}

#elifdef CONFIG_SOC_SF32LB52
void PBL_NOINLINE delay_us(uint32_t us) {
  HAL_Delay_us(us);
}

void delay_init(void) {
}

#elifdef CONFIG_QEMU
#include <cmsis_core.h>

void PBL_NOINLINE delay_us(uint32_t us) {
  // Use DWT cycle counter for accurate delays
  uint32_t cycles = us * (SystemCoreClock / 1000000);
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles) {
  }
}

void delay_init(void) {
  // Enable DWT cycle counter
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

#elifdef CONFIG_SOC_POSIX
void PBL_NOINLINE delay_us(uint32_t us) {
  usleep(us);
}

void delay_init(void) {
}
#endif
