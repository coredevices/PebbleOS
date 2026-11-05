/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/reboot.h>

#include <bf0_hal.h>

[[noreturn]] void pbl_soc_reset(void) {
  HAL_PMU_Reboot();
  unreachable();
}
