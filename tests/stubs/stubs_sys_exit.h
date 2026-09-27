/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <pbl/kernel/compiler.h>

#include <stdbool.h>

PBL_NORETURN void sys_exit(void) {
  cl_assert(false);
  while (true) {
  }
}
