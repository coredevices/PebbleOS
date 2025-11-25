/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"

void psleep(int millis) {
  sys_psleep(millis);
}

