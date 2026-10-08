/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>

#include <cmsis_core.h>
#include <kernel/util/idle.h>

void pbl_soc_idle(pbl_tick_t max_ticks) {
  if (!idle_is_allowed()) {
    return;
  }

  __disable_irq();

  if (pbl_idle_confirm()) {
    __DSB();
    __WFI();
    __ISB();
  }

  __enable_irq();
}

bool pbl_soc_tick_enable(void) {
  return false;
}

void pbl_analytics_external_collect_cpu_stats(void) {
}