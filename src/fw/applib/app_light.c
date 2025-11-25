/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"

void app_light_enable_interaction(void) {
  sys_light_enable_interaction();
}

void app_light_enable(bool enable) {
  sys_light_enable(enable);
}
