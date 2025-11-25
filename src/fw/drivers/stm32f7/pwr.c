/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pwr.h"

#include "drivers/periph_config.h"

#define STM32F7_COMPATIBLE
#include <mcu.h>

void pwr_enable_wakeup(bool enable) {
  if (enable) {
    __atomic_or_fetch(&PWR->CSR2, PWR_CSR2_EWUP1, __ATOMIC_RELAXED);
  } else {
    __atomic_and_fetch(&PWR->CSR2, ~PWR_CSR2_EWUP1, __ATOMIC_RELAXED);
  }
}

void pwr_flash_power_down_stop_mode(bool power_down) {
  if (power_down) {
    __atomic_or_fetch(&PWR->CR1, PWR_CR1_FPDS, __ATOMIC_RELAXED);
  } else {
    __atomic_and_fetch(&PWR->CR1, ~PWR_CR1_FPDS, __ATOMIC_RELAXED);
  }
}

void pwr_access_backup_domain(bool enable_access) {
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
  if (enable_access) {
    __atomic_or_fetch(&PWR->CR1, PWR_CR1_DBP, __ATOMIC_RELAXED);
  } else {
    __atomic_and_fetch(&PWR->CR1, ~PWR_CR1_DBP, __ATOMIC_RELAXED);
  }
  periph_config_disable(PWR, RCC_APB1Periph_PWR);
}
