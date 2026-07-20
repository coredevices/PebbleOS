/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/soc/soc_poweroff.h"

#include <bf0_hal.h>

#define WAKEUP_COUNT 0x50005

static void prv_set_wakeup_source() {
  HAL_PMU_SelectWakeupPin(0, HAL_HPAON_QueryWakeupPin(hwp_gpio1, 34));
  HAL_PMU_EnablePinWakeup(0, AON_PIN_MODE_HIGH);
  hwp_pmuc->WKUP_CNT = WAKEUP_COUNT;
}

NORETURN soc_poweroff(void) {
  prv_set_wakeup_source();
  HAL_PMU_EnterHibernate();

  /* Should not reach here */
  __builtin_unreachable();
}
