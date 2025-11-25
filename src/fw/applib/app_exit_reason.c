/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_exit_reason.h"

#include "syscall/syscall.h"

AppExitReason app_exit_reason_get(void) {
  return sys_process_get_exit_reason();
}

void app_exit_reason_set(AppExitReason exit_reason) {
  sys_process_set_exit_reason(exit_reason);
}
