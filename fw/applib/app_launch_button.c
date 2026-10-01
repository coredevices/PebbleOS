/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_launch_button.h"

#include "syscall/syscall.h"

ButtonId app_launch_button(void) {
  return sys_process_get_launch_button();
}
