/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <bf0_hal.h>

void soc_sf32lb52_lpsys_wake(void) {
  HAL_HPAON_WakeCore(CORE_ID_LCPU);
}

void soc_sf32lb52_lpsys_release(void) {
  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();
}
