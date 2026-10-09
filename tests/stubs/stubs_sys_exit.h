/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <clar.h>

[[noreturn]] void sys_exit(void) {
  cl_assert(false);
  while (true) {
  }
}
