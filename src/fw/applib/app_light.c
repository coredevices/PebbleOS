/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_light.h"

#include "syscall/syscall.h"

bool app_light_is_on(void) {
  return sys_light_is_on();
}

void app_light_enable_interaction(void) {
  sys_light_enable_interaction();
}

void app_light_enable(bool enable) {
  sys_light_enable(enable);
}

void app_light_set_color(GColor color) {
  sys_light_set_color(color.argb);
}

void app_light_set_system_color(void) {
  sys_light_set_system_color();
}
