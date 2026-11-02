/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/reboot.h>

PBL_WEAK void pbl_reboot(void) {
  PBL_ASSERT(false, "System reset triggered!");
}

PBL_WEAK void pbl_power_off(void) {
  PBL_ASSERT(false, "System power-off triggered!");
}

PBL_WEAK void pbl_reboot_emergency(void) {
  PBL_ASSERT(false, "System reset triggered!");
}
