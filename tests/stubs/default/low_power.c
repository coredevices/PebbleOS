/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

PBL_WEAK void low_power_standby(void) {
}

PBL_WEAK void low_power_enter(void) {
}

PBL_WEAK void low_power_exit(void) {
}

PBL_WEAK bool low_power_is_active(void) {
  return false;
}
