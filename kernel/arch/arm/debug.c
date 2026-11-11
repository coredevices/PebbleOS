/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <pbl/kernel/debug.h>

bool pbl_debugger_attached(void) {
  return (arch_readl(ARM_DHCSR) & ARM_DHCSR_C_DEBUGEN) != 0U;
}
