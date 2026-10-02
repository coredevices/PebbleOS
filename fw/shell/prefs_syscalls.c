/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "prefs_syscalls.h"

#include <pbl/kernel/syscall.h>

PBL_SYSCALL(UnitsDistance, sys_shell_prefs_get_units_distance) {
  return shell_prefs_get_units_distance();
}
