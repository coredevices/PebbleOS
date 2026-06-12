/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/attributes.h"

#include <bf0_hal.h>

NORETURN enter_hibernate(void) {
  HAL_PMU_EnterHibernate();

  /* Should not reach here */
  __builtin_unreachable();
}

