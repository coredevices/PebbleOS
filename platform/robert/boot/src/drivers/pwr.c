/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pwr.h"

#include "drivers/periph_config.h"

#include "stm32f7xx.h"

void pwr_access_backup_domain(bool enable_access) {
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
  if (enable_access) {
    __atomic_or_fetch(&PWR->CR1, PWR_CR1_DBP, __ATOMIC_RELAXED);
  } else {
    __atomic_and_fetch(&PWR->CR1, ~PWR_CR1_DBP, __ATOMIC_RELAXED);
  }
  periph_config_disable(PWR, RCC_APB1Periph_PWR);
}


bool pwr_did_boot_from_standby(void) {
  bool result = (PWR->CSR1 & PWR_CSR1_SBF) != 0;
  return result;
}

void pwr_clear_boot_from_standby_flag(void) {
  PWR->CR1 |= PWR_CR1_CSBF;
}
