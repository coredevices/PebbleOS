/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/soc/soc_poweroff.h"

NORETURN soc_poweroff(void) {
  /* Should not reach here */
  __builtin_unreachable();
}
