/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <cmsis_core.h>

#include "kernel/util/stop.h"

#include "FreeRTOS.h"
#include "task.h"

extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime ) {
  if (!sleep_mode_is_allowed()) {
    return;
  }

  __disable_irq();

  if (eTaskConfirmSleepModeStatus() != eAbortSleep) {
    __DSB();
    __WFI();
    __ISB();
  }

  __enable_irq();
}

bool vPortEnableTimer() {
  return false;
}

void dump_current_runtime_stats(void) {
}

void pbl_analytics_external_collect_cpu_stats(void) {
}