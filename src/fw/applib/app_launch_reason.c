/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_launch_reason.h"

#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"

AppLaunchReason app_launch_reason(void) {
  return sys_process_get_launch_reason();
}


uint32_t app_launch_get_args(void) {
  return sys_process_get_launch_args();
}
