/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <util/attributes.h>

#include <stdbool.h>

NORETURN sys_exit(void) {
  cl_assert(false);
  while (true) {}
}
